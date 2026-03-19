#!/usr/bin/env python3
#
# Copyright (C) 2024 GreenWaves Technologies, SAS, ETH Zurich and
#                    University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Generate RST documentation for GVSoC components by introspecting their Python generators.

This script imports component classes, inspects their __init__ parameters, docstrings,
and interface methods (i_* / o_*) to produce structured reference documentation.

Usage:
    python gen_component_docs.py [--output-dir DIR]

The script must be run after sourcing sourceme.sh so that PYTHONPATH includes the
models directory.
"""

import inspect
import importlib
import os
import sys
import re
import argparse
from dataclasses import dataclass
from typing import Optional


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class PortInfo:
    """Extracted information about a component port."""
    method_name: str        # e.g. "i_INPUT", "o_FETCH"
    direction: str          # "input" (slave) or "output" (master)
    signature: str          # interface signature, e.g. "io", "clock", "wire<bool>"
    description: str        # from docstring
    parameters: dict        # extra parameters (e.g. id for dynamic ports)


@dataclass
class PropertyInfo:
    """Extracted information about a component property."""
    name: str
    default: str
    description: str
    type_hint: str


@dataclass
class ComponentDoc:
    """All extracted documentation for a single component."""
    class_name: str
    module_path: str        # Python import path
    description: str        # class-level docstring (first paragraph)
    detailed_description: str  # full class docstring
    properties: list        # list of PropertyInfo
    ports: list             # list of PortInfo
    source_file: str        # relative path to .py file


# ---------------------------------------------------------------------------
# Extraction helpers
# ---------------------------------------------------------------------------

def extract_signature_from_source(method) -> Optional[str]:
    """Extract the interface signature from a method's source code.

    Looks for SlaveItf(..., signature='...') or itf_bind(..., signature='...') calls.
    """
    try:
        source = inspect.getsource(method)
    except (OSError, TypeError):
        return None

    # Match signature='...' or signature="..."
    match = re.search(r"signature=['\"]([^'\"]+)['\"]", source)
    if match:
        return match.group(1)

    # Match f-string signatures like signature=f'wire<...>'
    match = re.search(r"signature=f['\"]([^'\"]+)['\"]", source)
    if match:
        # Replace {expressions} with * to indicate parameterized signature
        sig = re.sub(r'\{[^}]+\}', '*', match.group(1))
        return sig
    return None


def extract_port_name_from_source(method) -> Optional[str]:
    """Extract the actual port/interface name from source code.

    Looks for SlaveItf(self, 'name', ...) or itf_bind('name', ...) patterns.
    """
    try:
        source = inspect.getsource(method)
    except (OSError, TypeError):
        return None

    # SlaveItf(self, 'portname', ...) or SlaveItf(self, itf_name='portname', ...)
    match = re.search(r"SlaveItf\(self,\s*(?:itf_name=)?['\"]([^'\"]+)['\"]", source)
    if match:
        return match.group(1)

    # itf_bind('portname', ...)
    match = re.search(r"itf_bind\(['\"]([^'\"]+)['\"]", source)
    if match:
        return match.group(1)

    return None


def parse_docstring_description(docstring: str) -> str:
    """Extract the description part from a numpydoc-style docstring.

    Returns everything before the first section header (Attributes, Parameters, Returns).
    Preserves paragraph breaks for RST output.
    """
    if not docstring:
        return ""

    lines = docstring.strip().split('\n')
    desc_lines = []
    for line in lines:
        stripped = line.strip()
        if stripped in ('Attributes', 'Parameters', 'Returns') or stripped.startswith('---'):
            break
        # Remove trailing \n markers used in gvsoc docstrings
        cleaned = stripped.rstrip('\\n').strip()
        desc_lines.append(cleaned)

    # Group into paragraphs: empty lines separate paragraphs
    paragraphs = []
    current = []
    for line in desc_lines:
        if line:
            current.append(line)
        else:
            if current:
                paragraphs.append(' '.join(current))
                current = []
    if current:
        paragraphs.append(' '.join(current))

    return '\n\n'.join(paragraphs)


def parse_numpydoc_attributes(docstring: str) -> dict:
    """Parse the Attributes section of a numpydoc-style docstring.

    Returns a dict mapping attribute name to its description.
    """
    if not docstring:
        return {}

    lines = docstring.split('\n')
    in_attributes = False
    attrs = {}
    current_name = None
    current_desc_lines = []

    for line in lines:
        stripped = line.strip()

        if stripped == 'Attributes':
            in_attributes = True
            continue

        if in_attributes and stripped.startswith('---'):
            continue

        if in_attributes:
            # Check for section end
            if stripped in ('Parameters', 'Returns', 'Examples', 'Notes'):
                if current_name:
                    attrs[current_name] = ' '.join(current_desc_lines).strip()
                break

            # Check for new attribute: "name: type" or "name : type"
            attr_match = re.match(r'^(\w+)\s*:\s*(.*)', stripped)
            if attr_match and not stripped.startswith(' '):
                # Save previous
                if current_name:
                    attrs[current_name] = ' '.join(current_desc_lines).strip()
                current_name = attr_match.group(1)
                current_desc_lines = []
            elif current_name and stripped:
                current_desc_lines.append(stripped.rstrip('\\n').strip())

    if current_name:
        attrs[current_name] = ' '.join(current_desc_lines).strip()

    return attrs


def extract_properties(cls) -> list:
    """Extract properties from a component class by inspecting __init__ parameters.

    Uses the class docstring's Attributes section for descriptions, and __init__
    signature for defaults and type hints.
    """
    # Get __init__ signature
    try:
        sig = inspect.signature(cls.__init__)
    except (ValueError, TypeError):
        return []

    # Parse attribute descriptions from class's own docstring (not inherited)
    docstring = inspect.cleandoc(cls.__doc__) if cls.__doc__ else ""
    attr_docs = parse_numpydoc_attributes(docstring)

    properties = []
    skip_params = {'self', 'parent', 'name', 'options', 'config', 'attributes'}

    for param_name, param in sig.parameters.items():
        if param_name in skip_params:
            continue

        # Get default value
        if param.default is inspect.Parameter.empty:
            default_str = "required"
        elif param.default is None:
            default_str = "None"
        else:
            default_str = repr(param.default)

        # Get type hint
        if param.annotation is not inspect.Parameter.empty:
            type_hint = getattr(param.annotation, '__name__', str(param.annotation))
        else:
            type_hint = ""

        # Get description from docstring
        description = attr_docs.get(param_name, "")

        properties.append(PropertyInfo(
            name=param_name,
            default=default_str,
            description=description,
            type_hint=type_hint,
        ))

    return properties


def extract_ports(cls) -> list:
    """Extract port information from i_* and o_* methods."""
    ports = []

    for name, method in inspect.getmembers(cls, predicate=inspect.isfunction):
        if not (name.startswith('i_') or name.startswith('o_')):
            continue

        # Skip base class ports (CLOCK, RESET, POWER, VOLTAGE) — they are inherited
        if name in ('i_CLOCK', 'i_RESET', 'i_POWER', 'i_VOLTAGE'):
            continue

        direction = "input" if name.startswith('i_') else "output"

        # Get signature from source
        signature = extract_signature_from_source(method) or "unknown"

        # Get description from docstring — flatten to single line for table cells
        docstring = inspect.getdoc(method) or ""
        description = parse_docstring_description(docstring).replace('\n\n', ' ').replace('\n', ' ')

        # Get extra parameters (for dynamic ports like i_INPUT(id=0))
        try:
            sig = inspect.signature(method)
            params = {
                k: v.default for k, v in sig.parameters.items()
                if k != 'self' and k != 'itf' and v.default is not inspect.Parameter.empty
            }
        except (ValueError, TypeError):
            params = {}

        ports.append(PortInfo(
            method_name=name,
            direction=direction,
            signature=signature,
            description=description,
            parameters=params,
        ))

    # Sort: inputs first, then outputs, alphabetically within each group
    ports.sort(key=lambda p: (0 if p.direction == "input" else 1, p.method_name))
    return ports


def extract_component_doc(cls, module_path: str, source_file: str) -> ComponentDoc:
    """Extract all documentation from a component class."""
    # Use __doc__ directly to avoid inheriting parent class docstring
    docstring = cls.__doc__ or ""
    docstring = inspect.cleandoc(docstring)
    description = parse_docstring_description(docstring)

    return ComponentDoc(
        class_name=cls.__name__,
        module_path=module_path,
        description=description,
        detailed_description=docstring,
        properties=extract_properties(cls),
        ports=extract_ports(cls),
        source_file=source_file,
    )


# ---------------------------------------------------------------------------
# RST generation
# ---------------------------------------------------------------------------

def rst_heading(text: str, char: str) -> str:
    """Generate an RST heading with underline."""
    return f"{text}\n{char * len(text)}\n"


def generate_component_rst(doc: ComponentDoc) -> str:
    """Generate RST documentation for a single component."""
    lines = []

    # Title
    lines.append(f".. _component_{doc.class_name.lower()}:\n")
    lines.append(rst_heading(doc.class_name, '='))
    lines.append("")

    # Module path
    lines.append(f"**Python module:** ``{doc.module_path}``\n")
    lines.append(f"**Source:** ``{doc.source_file}``\n")
    lines.append("")

    # Description
    if doc.description:
        lines.append(rst_heading("Description", '-'))
        lines.append("")
        lines.append(doc.description)
        lines.append("")

    # Properties table
    if doc.properties:
        lines.append(rst_heading("Properties", '-'))
        lines.append("")
        lines.append(".. list-table::")
        lines.append("   :header-rows: 1")
        lines.append("   :widths: 20 15 15 50")
        lines.append("")
        lines.append("   * - Name")
        lines.append("     - Type")
        lines.append("     - Default")
        lines.append("     - Description")

        for prop in doc.properties:
            lines.append(f"   * - ``{prop.name}``")
            lines.append(f"     - {prop.type_hint}")
            lines.append(f"     - ``{prop.default}``")
            lines.append(f"     - {prop.description}")

        lines.append("")

    # Ports
    if doc.ports:
        lines.append(rst_heading("Ports", '-'))
        lines.append("")

        # Input ports
        input_ports = [p for p in doc.ports if p.direction == "input"]
        output_ports = [p for p in doc.ports if p.direction == "output"]

        if input_ports:
            lines.append(rst_heading("Input ports (slave)", '~'))
            lines.append("")
            lines.append(".. list-table::")
            lines.append("   :header-rows: 1")
            lines.append("   :widths: 20 20 60")
            lines.append("")
            lines.append("   * - Method")
            lines.append("     - Signature")
            lines.append("     - Description")

            for port in input_ports:
                params_str = ""
                if port.parameters:
                    params_str = " — " + ", ".join(
                        f"``{k}={v}``" for k, v in port.parameters.items()
                    )
                lines.append(f"   * - ``{port.method_name}()``")
                lines.append(f"     - ``{port.signature}``")
                desc = port.description or ""
                if params_str:
                    desc += params_str
                lines.append(f"     - {desc}")

            lines.append("")

        if output_ports:
            lines.append(rst_heading("Output ports (master)", '~'))
            lines.append("")
            lines.append(".. list-table::")
            lines.append("   :header-rows: 1")
            lines.append("   :widths: 20 20 60")
            lines.append("")
            lines.append("   * - Method")
            lines.append("     - Signature")
            lines.append("     - Description")

            for port in output_ports:
                lines.append(f"   * - ``{port.method_name}()``")
                lines.append(f"     - ``{port.signature}``")
                lines.append(f"     - {port.description}")

            lines.append("")

    # Inherited ports note
    lines.append(".. note::")
    lines.append("")
    lines.append("   All components also inherit the following ports from ``Component``:")
    lines.append("")
    lines.append("   - ``i_CLOCK()`` — clock input (signature: ``clock``)")
    lines.append("   - ``i_RESET()`` — reset input (signature: ``wire<bool>``)")
    lines.append("   - ``i_POWER()`` — power supply control (signature: ``wire<int>``)")
    lines.append("   - ``i_VOLTAGE()`` — voltage control (signature: ``wire<int>``)")
    lines.append("")

    # Usage example
    lines.append(rst_heading("Usage example", '-'))
    lines.append("")
    lines.append(".. code-block:: python")
    lines.append("")
    lines.append(f"   import {doc.module_path.rsplit('.', 1)[0]}")
    lines.append("")
    lines.append(f"   # Instantiate in a parent component")

    # Build a simple instantiation example from required/notable properties
    example_args = []
    for prop in doc.properties:
        if prop.default == "required":
            if prop.type_hint == "int":
                example_args.append(f"{prop.name}=0x1000")
            elif prop.type_hint == "str":
                example_args.append(f'{prop.name}="value"')
            else:
                example_args.append(f"{prop.name}=...")
        elif prop.name in ('size', 'frequency', 'bandwidth') and prop.default != "required":
            # Show common properties even if they have defaults
            example_args.append(f"{prop.name}={prop.default}")

    args_str = ", ".join(example_args)
    if args_str:
        args_str = ", " + args_str
    lines.append(f"   comp = {doc.module_path.rsplit('.', 1)[1]}.{doc.class_name}(parent, 'name'{args_str})")
    lines.append("")

    # Show port binding examples
    if doc.ports:
        lines.append(f"   # Connect ports")
        for port in doc.ports[:3]:  # Show up to 3 examples
            if port.direction == "input":
                lines.append(f"   # master.o_XXX(comp.{port.method_name}())")
            else:
                lines.append(f"   # comp.{port.method_name}(slave.i_XXX())")
        lines.append("")

    return '\n'.join(lines)


def generate_index_rst(components: list) -> str:
    """Generate the index RST file for the component library."""
    lines = []
    lines.append(".. _component_library:\n")
    lines.append(rst_heading("Component Library", '='))
    lines.append("")
    lines.append("This section provides reference documentation for all GVSoC components.")
    lines.append("Each component page is auto-generated from the Python generator class,")
    lines.append("documenting its properties, ports, and usage.")
    lines.append("")
    lines.append("All components inherit from :class:`gvsoc.systree.Component` and follow the")
    lines.append("same patterns for instantiation and port binding. See :ref:`gvsoc_dev` for")
    lines.append("an introduction to the component model.")
    lines.append("")

    # Group components by category
    categories = {}
    for doc in components:
        # Derive category from module path
        parts = doc.module_path.split('.')
        if len(parts) >= 2:
            category = parts[0]
        else:
            category = "other"
        categories.setdefault(category, []).append(doc)

    for category, docs in sorted(categories.items()):
        lines.append(rst_heading(category.replace('_', ' ').title(), '-'))
        lines.append("")
        lines.append(".. toctree::")
        lines.append("   :maxdepth: 1")
        lines.append("")
        for doc in sorted(docs, key=lambda d: d.class_name):
            filename = f"components/{doc.class_name.lower()}"
            lines.append(f"   {filename}")
        lines.append("")

    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Component registry
# ---------------------------------------------------------------------------

# Components to document: (module_path, class_name, source_file)
COMPONENTS = [
    ("memory.memory", "Memory", "core/models/memory/memory.py"),
    ("interco.router", "Router", "core/models/interco/router.py"),
    ("vp.clock_domain", "Clock_domain", "core/models/vp/clock_domain.py"),
    ("cpu.iss.riscv", "RiscvCommon", "core/models/cpu/iss/riscv.py"),
    ("devices.uart.ns16550", "Ns16550", "core/models/devices/uart/ns16550.py"),
]


def main():
    parser = argparse.ArgumentParser(description='Generate GVSoC component documentation')
    parser.add_argument('--output-dir',
        default=os.path.join(os.path.dirname(__file__), 'components'),
        help='Output directory for generated RST files')
    parser.add_argument('--dry-run', action='store_true',
        help='Print to stdout instead of writing files')
    parser.add_argument('--components', nargs='*',
        help='Only generate docs for these component class names')
    args = parser.parse_args()

    component_docs = []

    for module_path, class_name, source_file in COMPONENTS:
        if args.components and class_name not in args.components:
            continue

        try:
            module = importlib.import_module(module_path)
            cls = getattr(module, class_name)
        except (ImportError, AttributeError) as e:
            print(f"WARNING: Could not import {module_path}.{class_name}: {e}",
                  file=sys.stderr)
            continue

        doc = extract_component_doc(cls, module_path, source_file)
        component_docs.append(doc)
        print(f"Extracted: {doc.class_name} "
              f"({len(doc.properties)} properties, {len(doc.ports)} ports)")

    if args.dry_run:
        for doc in component_docs:
            print("=" * 70)
            print(generate_component_rst(doc))
        print("=" * 70)
        print(generate_index_rst(component_docs))
        return

    # Write files
    os.makedirs(args.output_dir, exist_ok=True)

    for doc in component_docs:
        filename = os.path.join(args.output_dir, f"{doc.class_name.lower()}.rst")
        with open(filename, 'w') as f:
            f.write(generate_component_rst(doc))
        print(f"  Wrote {filename}")

    # Write index
    index_file = os.path.join(os.path.dirname(args.output_dir), 'component_library.rst')
    with open(index_file, 'w') as f:
        f.write(generate_index_rst(component_docs))
    print(f"  Wrote {index_file}")

    print(f"\nGenerated documentation for {len(component_docs)} components.")


if __name__ == '__main__':
    main()
