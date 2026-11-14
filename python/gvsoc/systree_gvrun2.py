#
# Copyright (C) 2022 GreenWaves Technologies, SAS, ETH Zurich and
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

from dataclasses import fields, is_dataclass
from typing import get_type_hints
import logging
import gvsoc.json_tools as js
import collections
import os
import json
import sys
import shutil
from importlib import import_module
import gvrun.target
import gvsoc.gui
import hashlib
import gvsoc.generated_sources
import rich.table
from gvrun.runtime import is_runtime_annotation
from gvrun.parameter import TargetParameter
from gvsoc.signature import Signature


generated_components = {}


def _runtime_value_str(value, cpp_type: str) -> str:
    """Format one runtime config value in the canonical text form the engine
    parses back (see vp::RuntimeConfig)."""
    if cpp_type == 'bool':
        return 'true' if value else 'false'
    if cpp_type == 'int64_t':
        return str(int(value))
    if cpp_type == 'double':
        return repr(float(value))
    text = str(value)
    if '\n' in text:
        raise ValueError(f'Runtime config string values cannot contain newlines: {text!r}')
    return text


def dump_runtime_config(component, file_path: str):
    """Write the per-run runtime config key/value file for the given systree.

    The file carries the values of all ``Annotated[T, Runtime]`` config
    fields, keyed by ``<component path>/<field>``. It is regenerated at every
    run and read by the engine (--runtime-config), which overlays the values
    onto the compiled tree config structs at component construction.
    """
    lines = []
    component._dump_runtime_fields(lines)
    with open(file_path, 'w', encoding='utf-8') as file_desc:
        for line in lines:
            file_desc.write(line + '\n')


def _signatures_match(master_sig, slave_sig):
    """Bind-time compatibility check between two signatures.

    Accepts any combination of legacy strings and :class:`Signature`
    instances. Two strings match by equality. A ``Signature`` matches a
    string when the signature's ``tag`` equals the string (or vice versa).
    Two ``Signature`` instances match when ``master_sig.is_compatible(slave_sig)``
    or the auto-bridge pass can handle the mismatch later; here we only
    block bindings that are definitively unrelated (e.g. an ``io`` master
    on a ``wire<bool>`` slave). That means: same tag, or master is a
    ``Signature`` whose tag matches the slave's tag/string.
    """
    if master_sig is None or slave_sig is None:
        return True

    master_tag = master_sig.tag if isinstance(master_sig, Signature) else master_sig
    slave_tag = slave_sig.tag if isinstance(slave_sig, Signature) else slave_sig
    return master_tag == slave_tag


class GeneratedComponent(object):

    def __init__(self, name, comp_name, sources, cflags, libs):
        self.sources = sources
        self.cflags = cflags
        self.libs = libs
        self.config_name = name
        self.name = f'gen_{comp_name}'
        self.comp_name = comp_name
        self.full_name = name



# This takes care of computing if a new module has to be compiled for the specified set
# of sources, cflags and libs, so that we can reuse one module for several instances
# This is only for components which are fully handled from Python without cmake
def get_generated_component(sources, cflags, libs):
    # Compute full name from sources, cflags and libs
    name = ''.join(sources + cflags + libs)
    # Compute a unique hash so that we don't compile with long name
    name_hash = int(hashlib.md5(name.encode('utf-8')).hexdigest()[0:7], 16)

    # Loop until we find a hash which does not collide with existing components
    while True:
        # We also put first source code in the name so that the user can understand a bit
        # what is being compiled.
        # Remove characters which prevent cmake from compiling
        template_name = gvsoc.generated_sources.component_label(sources[0])
        comp_name = f'{template_name}_{name_hash}'

        # Check if this collides
        generated_component = generated_components.get(comp_name)
        if generated_component is None or generated_component.full_name == name:
            break

        name_hash += 1

    # If the component is not yet registered do it
    if generated_components.get(comp_name) is None:
        generated_components[comp_name] = GeneratedComponent(name=name, comp_name=comp_name, sources=sources, cflags=cflags, libs=libs)

    return generated_components[comp_name]


class SlaveItf():
    """Slave interface

    This can be used to instantiate a slave interface which can be used to bind a master to it
    using the itf_bind method.\n

    Attributes
    ----------
    component : Component
        The component which the slave interface belongs to.
    itf_name: str
        The name of the slave interface.
    signature: str or Signature
        The signature of the interface. Accepts either the legacy string form
        (e.g. ``'io_v2'``, ``'wire<bool>'``) or a class-based ``Signature``
        instance (e.g. ``IoV2Beat(beat_width=64)``). When both sides of a
        binding declare class-based signatures, the framework auto-inserts a
        bridge component if the master signature reports incompatibility.
    """
    def __init__(self, component: 'Component', itf_name: str, signature=None):
        self.component = component
        self.itf_name = itf_name
        self.signature = signature

    def tie(self, value):
        """Tie this wire input to a constant, like an input tied to '0 or '1 in
        the RTL.

        A tie cell is added next to the component and drives the value at every
        reset release.

        Parameters
        ----------
        value : int or bool
            Constant driven on the input.
        """
        from utils.tie import Tie
        comp = self.component
        if comp.parent is None:
            raise RuntimeError(f'Cannot tie an input of the top component ({self.itf_name})')
        cell = Tie(comp.parent, f'{comp.name}_{self.itf_name}_tie', value=value,
            signature=self.signature)
        cell.o_OUT(self)


class Port():

    def __init__(self, comp, name):
        self.name = name
        self.comp = comp
        self.remote_slave_ports = []
        self.remote_master_ports = []
        self.properties = {}

    def get_name(self):
        return self.name

    def get_properties(self, as_master=True, as_slave=True):

        properties = self.properties.copy()

        if len(self.properties) != 0:
            properties.update(self.properties)

        if as_slave:
            for port in self.remote_master_ports:
                properties.update(port.get_properties(as_master=False, as_slave=True))

        if as_master:
            for port in self.remote_slave_ports:
                properties.update(port.get_properties(as_master=True, as_slave=False))

        return properties

    def bind(self, remote_port, master_properties, slave_properties):

        self.remote_slave_ports.append(remote_port)
        if master_properties is not None:
            self.properties.update(master_properties)

        remote_port.remote_master_ports.append(self)
        if slave_properties is not None:
            remote_port.properties.update(slave_properties)


class Interface:

    def __init__(self, comp, name, properties):
        self.comp = comp
        self.name = name
        self.remote_itf = None
        self.properties = properties

    def bind(self, remote_itf):
        self.remote_itf = remote_itf
        self.comp.parent.interfaces.append(self)


class Component(gvrun.target.SystemTreeNode):
    """
    This class corresponds to a generic HW component from which any component should inherit.

    Attributes
    ----------
    parent : Component
        The component which this components belongs to. Can be None if the component is the top one.
    name: str
        Name of the component. This names is used to indentify this component in the parent component.
    options: list
        List of options of forms key=value which should overwrite component properties.
    """

    def __init__(self, parent: 'Component', name: str, config=None, options=None):
        super().__init__(name, parent=parent, config=config)
        self.parent = parent
        self._component_config = config
        self.components = {}
        self.properties = {}
        self.bindings = []
        self.ports = {}
        self.build_done = False
        self.finalize_done = False
        self.is_top = False
        self.vcd_group_create = True
        self.vcd_group_closed = True
        self.component = None
        self.interfaces = []
        self.c_flags = []
        self.sources = []
        self.libs = []
        self.vcd_traces = []
        self.gui_callbacks = []

        if parent is not None and isinstance(parent, Component):
            parent.__add_component(name, self)

        if config is not None:
            from gvrun.systree import get_attribute_arg_value
            for f in fields(config):
                # Only process the fields which has been assigned
                if hasattr(config, f.name):
                    path = self.get_path() + '/' + f.name
                    self.__apply_cmdline_parameter(config, f.name, path)
                    value = getattr(config, f.name)
                    if isinstance(value, (int, bool, str)):
                        # Check for attribute override using the component path
                        override = get_attribute_arg_value(path)
                        if override is not None:
                            value = type(value)(override)
                            setattr(config, f.name, value)
                        self.add_property(f.name, value)

    def __apply_cmdline_parameter(self, config, field_name: str, path: str):
        """Apply the --parameter value given for a config field, if any.

        Only runtime fields (``Annotated[T, Runtime]``) can be set this way: they
        are passed to the engine at each run, while the other fields are compiled
        into the platform tree and are set in the target string (attr.).
        """
        from gvrun.parameter import get_cmdline_parameter_value
        value = get_cmdline_parameter_value(path)
        if value is None:
            return
        from gvrun.config_gen import get_config_fields
        from gvrun.runtime import unwrap_annotated
        from config_tree.field_type_alias import cast_to_type
        is_runtime = False
        for fld in get_config_fields(type(config)):
            if fld['name'] == field_name:
                is_runtime = fld['runtime']
                break
        if not is_runtime:
            raise RuntimeError(
                f"--parameter {path} sets a config field compiled into the platform: give it "
                f"in the target string instead (--target <target>:attr.{path}=<value>), or "
                f"declare the field Annotated[T, Runtime] to set it at run time")
        field_type = get_type_hints(type(config), include_extras=True).get(field_name)
        setattr(config, field_name, cast_to_type(value, unwrap_annotated(field_type),
            config=config))


    def generate(self, builddir):
        pass

    def generate_all(self, builddir):
        for component in self.components.values():
            component.generate_all(builddir)

        self.generate(builddir)

    def configure(self):
        pass

    def configure_all(self):
        for component in self.components.values():
            component.configure_all()

        self.configure()

    def add_vcd_trace(self, path: str, trace_type: str, gui: str=None, display=None):
        """Declare a user VCD trace.

        This registers a VCD trace that the GVSoC engine will create at startup.
        The trace can then be opened from the simulated software using
        gv_vcd_open_trace(). The gvrun option --vcd-trace declares them the same way.

        Parameters
        ----------
        path : str
            Path of the trace (e.g. 'kernel/active').
        trace_type : str
            Type of the trace: 'int' for integer values, 'real' for real values (written with
            integers, shown as an analog curve), 'string' for string values.
        gui : str, optional
            Where to show the trace in the GUI signal tree, as a path of groups ending with
            the signal name (e.g. 'kernels/active'). Not shown by this method if None, the
            caller can then add the signal itself with add_gui_callback.
        display : optional
            Display of the GUI signal (gvsoc.gui.Display* object), defaults to a box for
            integers, to an analog curve for reals and to a string box for strings.
        """
        # Declared again (e.g. both in config.py and on the command line): the last one wins
        self.vcd_traces = [trace for trace in self.vcd_traces if trace['path'] != path]
        self.vcd_traces.append({'path': path, 'type': trace_type, 'gui': gui, 'display': display})

    def get_vcd_traces(self):
        """Get all declared user VCD traces.

        Returns
        -------
        list[dict]
            List of trace descriptors, each with 'path' and 'type' keys.
        """
        return self.vcd_traces

    def add_gui_callback(self, callback):
        """Register a callback for GUI signal generation.

        The callback will be called during gen_gui_stub with the parent signal
        as argument: callback(parent_signal). It can use gvsoc.gui.Signal and
        related classes to add custom signals to the GUI.

        Parameters
        ----------
        callback : callable
            Function taking (parent_signal) as argument.
        """
        self.gui_callbacks.append(callback)

    def i_RESET(self) -> SlaveItf:
        return SlaveItf(self, 'reset', signature='wire<bool>')

    def i_CLOCK(self) -> SlaveItf:
        """Returns the clock port.

        A clock generator should be bound on this port if this component is clocked.\n
        It is not needed if the component is asynchronous.\n
        It instantiates a port of type vp::ClkSlave.\n

        Returns
        ----------
        SlaveItf
            The slave interface
        """
        return SlaveItf(self, 'clock', signature='clock')

    def i_POWER(self) -> SlaveItf:
        """Returns the power port.

        Returns the port for controlling component power.\n
        It instantiates a port of type vp::WireSlave<int>.\n

        Returns
        ----------
        SlaveItf
            The slave interface
        """
        return SlaveItf(self, 'power_supply', signature='wire<int>')

    def i_VOLTAGE(self) -> SlaveItf:
        """Returns the power port.

        Returns the port for controlling component voltage.\n
        It instantiates a port of type vp::WireSlave<int>.\n

        Returns
        ----------
        SlaveItf
            The slave interface
        """
        return SlaveItf(self, 'voltage', signature='wire<int>')

    def add_property(self, name: str, property: str, format: type=None):
        """Add a property.

        A property is made available to the C++ model.

        Parameters
        ----------
        name : str
            Name of the property.

        property : str, int, float, list or dict
            Value of the property.
        """
        properties = self.properties

        for item in name.split('/')[:-1]:
            if properties.get(item) is None:
                properties[item] = {}

            properties = properties.get(item)

        properties[name.split('/')[-1]] = property

        return self.get_property(name, format=format)

    def add_properties(self, properties: dict):
        """Add properties.

        Properties are made available to the C++ model.\n
        This adds several properties at once, trough a dictionary, whose keys are the property
        names, and the values the values of the properties.\n

        Parameters
        ----------
        properties : dict
            Dictionary containing the properties to be added.
        """
        self.properties.update(properties)


    def add_sources(self, sources: list):
        """Add sources.

        The specified sources are added to the component.\n
        They will be automatically compiled when the component is compiled.\n
        Note that if the same component is instantiated several times with different sources, it
        will be compiled once for each set of sources.\n

        Parameters
        ----------
        sources : list
            List of source files to be added. Their path can be relative, in which case they will
            be searched from the platform target directories.
            A bare file name found next to the Python file calling this method is taken from
            there, so that two directories can use the same file names.
        """
        self.sources += gvsoc.generated_sources.qualify_sources(
            sources, sys._getframe(1).f_code.co_filename)

    def add_c_flags(self, flags):
        """Add C flags.

        The specified C flags are added to the component and use to compile all the sources
        of the component.
        Note that if the same component is instantiated several times with different C flags, it
        will be compiled once for each set of C flags.\n

        Parameters
        ----------
        flags : list
            List of C flags to be added.
        """
        self.c_flags += flags

    def add_libraries(self, libs: list):
        """Add link libraries.

        The specified libraries are linked into the shared library compiled from this
        component's sources. Each entry is passed to the linker, so it can be a library
        name (e.g. ``m`` to link ``libm``), a full path to a ``.so``/``.a``, or a linker
        flag (e.g. ``-lfoo`` or ``-L/path``).\n
        This only applies to components fully handled from Python (i.e. with sources added
        through add_sources, without a CMakeLists).\n
        Note that if the same component is instantiated several times with different
        libraries, it will be compiled once for each set of libraries.\n

        Parameters
        ----------
        libs : list
            List of libraries to be linked.
        """
        self.libs += libs

    def itf_bind(self, master_itf_name: str, slave_itf: SlaveItf, signature=None,
            composite_bind: bool=False, clock_bridge=None):
        """Bind to a slave interface.

        The specified master interface of this component is bound to the specified
        slave interface.\n
        A signature can be specified in order to make sure it is bound to an interface
        of the same signature.

        Parameters
        ----------
        master_itf_name : str
            Name of the master interface to be bound.
        slave_itf : SlaveItf
            Slave interface to which the master interface should be bound.
        signature : str or Signature
            The signature of the interface. Accepts either the legacy string
            form or a class-based ``Signature`` instance. When both sides
            declare class-based signatures, the framework auto-inserts a
            bridge component if the master signature reports incompatibility
            with the slave signature.
        """

        if signature is not None and slave_itf.signature is not None and \
                not _signatures_match(signature, slave_itf.signature):
            def _label(sig):
                return type(sig).__name__ if isinstance(sig, Signature) else sig
            master_name = f'{_label(signature)}@{self.get_path()}->{master_itf_name}'
            slave_name = f'{_label(slave_itf.signature)}@{slave_itf.component.get_path()}->{slave_itf.itf_name}'
            raise RuntimeError(f'Invalid signature (master: {master_name}, slave: {slave_name})')

        if composite_bind:
            self.bind(self, master_itf_name, slave_itf.component, slave_itf.itf_name,
                      master_signature=signature, slave_signature=slave_itf.signature,
                      clock_bridge=clock_bridge)
        else:
            self.parent.bind(self, master_itf_name, slave_itf.component, slave_itf.itf_name,
                             master_signature=signature, slave_signature=slave_itf.signature,
                             clock_bridge=clock_bridge)

    def regmap_gen(self, template, outdir, name, block=None, headers=['regfields', 'gvsoc']):
        import regmap.regmap
        import regmap.regmap_md
        import regmap.regmap_c_header
        outfile = f'{outdir}/{name}'
        logging.debug(f'Generating regmap (template: {template}, file: {outfile}*)')
        regmap_instance = regmap.regmap.Regmap(name)
        regmap.regmap_md.import_md(regmap_instance, template, block=block)
        regmap.regmap_c_header.dump_to_header(regmap=regmap_instance, name=name,
            header_path=outfile, headers=headers)

    def __add_component(self, name, component):
        """Add a new component.

        The new component will be a sub-component of this component and will be identified by the specified name

        Parameters
        ----------
        name : str
            Name of the component.
        component: Component
            Component

        Returns
        -------
        Component
            The component
        """
        self.components[name] = component
        component.name = name

        return component


    def get_component(self, name):
        """Get a new component.

        Parameters
        ----------
        name : str
            Name of the component.

        Returns
        -------
        Component
            The component
        """
        if name.find('/') != -1:
            local_name, child_name = name.split('/', 1)
            return self.components[local_name].get_component(child_name)

        return self.components[name]



    def get_property(self, name, format=None):
        """Get a property.

        The value of the property returned here can be overwritten by an option.

        Parameters
        ----------
        name : str
            Name of the property.

        Returns
        -------
        str, int, float, list or dict
            Value of the property.
        """
        option_property = None

        property = None
        for item in name.split('/'):
            if property is None:
                property = self.properties.get(item)
            else:
                property = property.get(item)

        if format is not None:
            if format == int:
                if type(property) == str:
                    property = int(property, 0)
                else:
                    property = int(property)

        return property


    def bind(self, master, master_itf, slave, slave_itf, master_properties=None, slave_properties=None,
             master_signature=None, slave_signature=None, clock_bridge=None):
        """Binds 2 components together.

        The binding can actually also involve 1 or 2 ports of the component to model bindings with something
        outside the component.

        Parameters
        ----------
        master : Component
            Master component. Can also be self if the master is outside this component.
        master_itf : str
            Name of the port where the binding should be done on master side.
        slave : Component
            Slave component. Can also be self if the slave is outside this component.
        slave_itf : str
            Name of the port where the binding should be done on slave side.
        master_signature : str or Signature
            Optional master-side signature (class-based or legacy string).
        slave_signature : str or Signature
            Optional slave-side signature (class-based or legacy string).
        clock_bridge : str | tuple(str, dict) | None
            Optional per-binding override of the CDC bridge kind for v2 IO
            bindings that cross clock domains. ``None`` falls back to
            ``set_clock_bridge_policy`` and finally to ``'sync_only'``.
        """
        self.bindings.append([master, master_itf, slave, slave_itf, master_properties, slave_properties,
                              master_signature, slave_signature, clock_bridge])

    def set_clock_bridge_policy(self, src_clock, dst_clock, kind, opts=None):
        """Declare which CDC bridge kind to insert between two clock sources.

        Used by the v2 IO auto-bridge pass when a binding's master and slave
        resolve to ``src_clock`` and ``dst_clock`` respectively and no
        per-binding ``clock_bridge=`` override is set on the binding.
        """
        if not hasattr(self, '_clock_bridge_policy'):
            self._clock_bridge_policy = {}
        self._clock_bridge_policy[(id(src_clock), id(dst_clock))] = (kind, opts or {})

    def get_clock_bridge_policy(self, src_clock, dst_clock):
        """Look up a previously declared policy. Returns ``(kind, opts)`` or ``None``."""
        policy = getattr(self, '_clock_bridge_policy', None)
        if policy is None:
            return None
        return policy.get((id(src_clock), id(dst_clock)))

    def _resolve_clock_source(self):
        """Walk up the parent chain to find the (clock_source, port) bound to
        this component's ``clock`` slave port. Returns ``None`` if no clock
        binding has been declared anywhere up the chain.
        """
        comp = self
        while comp is not None:
            parent = getattr(comp, 'parent', None)
            if parent is None or not hasattr(parent, 'bindings'):
                return None
            for binding in parent.bindings:
                if binding[2] is comp and binding[3] == 'clock':
                    return (binding[0], binding[1])
            comp = parent
        return None

    def slave_itf(self, name: str, signature: str):
        return SlaveItf(self, name, signature=signature)

    def load_property_file(self, path):
        """Loads a JSON property file.

        The file is imported as a python disctionary equivalent to the JSON file

        Returns
        -------
        dict
            The resulting dictionary
        """

        with open(self.get_file_path(path), 'r') as fd:
            return json.load(fd)

    def set_component(self, name):
        self.component = name
        self.add_property('vp_component', name)

    def get_generated_components(self):
        return generated_components

    def get_component_list(self, c_flags=None):
        result = []

        if c_flags is not None and len(self.c_flags) != 0:
            c_flags[self.component] = self.c_flags

        if self.component is not None:
            result.append(self.component)

        def Union(lst1, lst2):
            final_list = list(set(lst1) | set(lst2))
            return final_list

        for child in self.components.values():
            result = Union(result, child.get_component_list(c_flags))

        return result

    def _dump_runtime_fields(self, lines: list):
        """Append the runtime config lines of this component and its whole
        sub-hierarchy to ``lines``.

        One ``<component path>/<field>=<value>`` line per
        ``Annotated[T, Runtime]`` config field; a list field's own key
        carries the element count and each element follows as
        ``<component path>/<field>/<index>/<subfield>=<value>``. The engine
        overlays these values onto the compiled tree configs at component
        construction, so runtime values never enter the compiled tree nor
        its signature.
        """
        config = getattr(self, '_component_config', None)
        if config is not None and is_dataclass(config):
            from gvrun.config_gen import get_config_fields
            path = self.get_path()
            prefix = f'{path}/' if path != '' else ''
            for fld in get_config_fields(type(config)):
                if not fld['runtime']:
                    continue
                value = getattr(config, fld['name'], None)
                if value is None:
                    continue
                key = f'{prefix}{fld["name"]}'
                if fld['cpp_type'] == 'list':
                    lines.append(f'{key}={len(value)}')
                    elem_fields = [f for f in get_config_fields(fld['list_elem_cls'])
                        if f['cpp_type'] != 'list']
                    for index, elem in enumerate(value):
                        for elem_fld in elem_fields:
                            elem_value = getattr(elem, elem_fld['name'], None)
                            if elem_value is None:
                                continue
                            lines.append(f'{key}/{index}/{elem_fld["name"]}='
                                f'{_runtime_value_str(elem_value, elem_fld["cpp_type"])}')
                else:
                    lines.append(f'{key}={_runtime_value_str(value, fld["cpp_type"])}')

        for child in self.components.values():
            child._dump_runtime_fields(lines)

    def get_config(self):
        """Return the typed config dataclass attached to this component.

        Returns
        -------
        Config | None
            The dataclass instance passed to the component at construction time, or
            None if no config was provided.
        """
        return self.get_attributes()

    def get_json_config(self):
        """Generates and return the system configuration.

        The whole hierarchy of components, bindings and properties are converted to a dictionnary
        representing the system, which can be serialized to a JSON file.

        Returns
        -------
        dict
            The resulting configuration
        """
        if not self.build_done:
            self.__build()

        if not self.finalize_done:
            self.__finalize()

        config = {}

        for component_name, component in self.components.items():
            config[component_name] = component.get_json_config()

        # A property named like a sub-component (e.g. a config field enabling
        # it) must not replace the sub-component's own config
        config.update({name: value for name, value in self.properties.items()
            if name not in self.components})

        if len(self.bindings) != 0:
            if config.get('bindings') is None:
                config['bindings'] = []
            for binding in self.bindings:
                master_name = 'self' if binding[0] == self else binding[0].name
                slave_name = 'self' if binding[2] == self else binding[2].name
                config['bindings'].append([ '%s->%s' % (master_name, binding[1]), '%s->%s' % (slave_name, binding[3])])

        if len(self.components.values()) != 0:
            config['components'] = list(self.components.keys())

        if len(self.ports) != 0:
            config['ports'] = list(self.ports.keys())

        return config


    def get_file_path(self, json):
        """Return absolute config file path.

        The specified file is search from PYTHONPATH.

        Returns
        -------
        string
            The absolute path or None is the file is not found
        """

        for dirpath in sys.path:
            path = os.path.join(dirpath, json)
            if os.path.exists(path):
                return path

        return None


    def vcd_group(self, closed=True, skip=False):
        self.vcd_group_create = not skip
        self.vcd_group_closed = closed

    def gen_gui_stub(self, parent_signal):
        parent_signal = self.gen_gui(parent_signal)

        for callback in self.gui_callbacks:
            callback(parent_signal)

        for component in self.components.values():
            component.gen_gui_stub(parent_signal)

    def gen_gui(self, parent_signal):
        name = self.get_name()
        if name is not None:
            # By default, we always generate a combiner but we could do it only if a dedicated
            # option is there
            # signal = gvsoc.gui.Signal(self, parent_signal, name=name, skip_if_no_child=True)
            # return signal
            return gvsoc.gui.SignalGenFromSignals(self, parent_signal, to_signal=name,
                mode="combined", from_groups=["active"], groups=["regmap", "active"],
                display=gvsoc.gui.DisplayLogicBox('ACTIVE'), skip_if_no_child=True,)
        else:
            return parent_signal

    def gen_gtkw_tree(self, tree, traces, filter_traces=False):
        if filter_traces:
            comp_traces = []
            subcomps_traces = []

            for trace in traces:
                if trace[0][0] == self.get_name():
                    if len(trace[0][1:]) == 0:
                        comp_traces.append([trace[0][1:], trace[1], trace[2]])
                    subcomps_traces.append([trace[0][1:], trace[1], trace[2]])
        else:
            comp_traces = []
            subcomps_traces = traces

        self.gen_gtkw_conf(tree, comp_traces)

        if self.vcd_group_create or tree.gen_full_tree:
            tree.begin_group(self.get_name(), closed=self.vcd_group_closed)

        self.__gen_gtkw_wrapper(tree, comp_traces)

        for component in self.components.values():

            component.gen_gtkw_tree(tree, traces=subcomps_traces, filter_traces=self.is_top or filter_traces)

        if self.vcd_group_create or tree.gen_full_tree:
            tree.end_group(self.get_name(), closed=self.vcd_group_closed)

    def __gen_traces(self, tree, traces):
        groups = {}
        for trace in traces:
            if len(trace[1]) == 1:
                tree.add_trace(self, trace[1][0], '.'.join(trace[0]) + trace[2])
            else:
                if groups.get(trace[1][0]) is None:
                    groups[trace[1][0]] = []

                groups[trace[1][0]].append([trace[0], trace[1][1:], trace[2]])

        for group_name, group in groups.items():
            tree.begin_group(group_name)
            self.__gen_traces(tree, group)
            tree.end_group(group_name)

    def __gen_gtkw_wrapper(self, tree, traces):
        if tree.gen_full_tree:
            self.__gen_traces(tree, traces)
        else:
            self.gen_gtkw(tree, traces)

    def gen_gtkw_conf(self, tree, traces):
        pass

    def gen_gtkw(self, tree, traces):
        pass

    def __add_port(self, name):
        port = self.ports.get(name)
        if  port is None:
            port = Port(self, name)
            self.ports[name] = port
        return port

    def get_itf(self, name, properties=None):
        return Interface(self, name, properties)

    def get_ports(self):
        return list(self.ports.values())

    def __build(self):

        # Auto-insert signature bridges for itf_bind() bindings. This must run
        # before children are recursed (so the rewritten bindings are visible
        # at this level) and before the Interface-path bindings are appended
        # (those use the legacy mechanism and are not subject to bridging).
        self.__expand_signature_bridges()

        for interface in self.interfaces:
            self.bindings.append([interface.comp, interface.name, interface.remote_itf.comp,
                interface.remote_itf.name, interface.properties, interface.remote_itf.properties,
                None, None])

        for component in self.components.values():
            component.__build()

        if len(self.bindings) != 0:
            for binding in self.bindings:
                master_port = binding[0].__add_port(binding[1])
                slave_port = binding[2].__add_port(binding[3])
                master_port.bind(slave_port, master_properties=binding[4], slave_properties=binding[5])

        self.build_done = True

        if self.component is None and len(self.sources) != 0:
            self.generated_component = get_generated_component(self.sources, self.c_flags, self.libs)
            self.add_property('vp_component', self.generated_component.name)

    def __expand_signature_bridges(self):
        """Walk this component's bindings and insert auto-bridge components
        where the master signature reports incompatibility with the slave.

        The master signature must be a class-based :class:`Signature` instance
        for the framework to consider auto-bridging. The slave signature can
        be either a :class:`Signature` instance or a legacy string — leaving
        a fallback for masters that upgraded to the class-based API while the
        bound slave is still declared with the historic ``signature='...'``
        string. The master signature's ``bridge_to`` decides what (if anything)
        to insert.

        Bindings with no class-based master signature are left untouched.

        The bridge component registers itself as a child of ``self`` via the
        normal ``Component.__init__`` parent argument. The two replacement
        bindings inherit the master signature on the master leg and the slave
        signature on the slave leg, so the recursive expansion is idempotent.
        Each inserted bridge also gets a clock binding cloned from the master
        component's existing clock binding so the framework-injected bridge
        is fully clocked without the user having to wire it manually.
        """
        from gvsoc.signature import IoV2BigPacket, IoV2Sync, IoV2Beat
        from gvsoc.signature import assert_io_v2_strict
        from gvsoc import clock_bridges
        expanded = []
        # Pre-index existing clock bindings so we can clone the master's
        # clock for any bridge we insert.
        clock_by_comp = {}
        for binding in self.bindings:
            if binding[3] == 'clock':
                # binding shape: [src, src_port, dst_comp, 'clock', ..., ..., ..., ...]
                clock_by_comp[binding[2]] = (binding[0], binding[1])

        new_clock_bindings = []
        for binding in self.bindings:
            master_sig = binding[6]
            slave_sig = binding[7]
            # Strict-protocol gate: forbid the retired generic big-packet
            # io_v2 (bare 'io_v2' string or IoV2BigPacket) on either end,
            # unless the escape hatch is enabled. Runs before the
            # isinstance(Signature) skip so a bare-string master is caught too.
            assert_io_v2_strict(
                master_sig, slave_sig,
                f'{binding[0].get_path()}:{binding[1]} -> '
                f'{binding[2].get_path()}:{binding[3]}')
            if not isinstance(master_sig, Signature):
                expanded.append(binding)
                continue

            bridge_name = f'{binding[0].name}_{binding[1]}_bridge'
            try:
                bridge = master_sig.bridge_to(slave_sig, parent=self, name=bridge_name)
            except RuntimeError as e:
                raise RuntimeError(
                    f'{e} (binding: {binding[0].get_path()}:{binding[1]} -> '
                    f'{binding[2].get_path()}:{binding[3]})') from e
            if bridge is None:
                expanded.append(binding)
                continue

            # Rewrite the binding pair to route master -> bridge.input ->
            # bridge.output -> slave. The bridge's own ports use the same
            # signature on each end, so the next expansion pass over those
            # bindings is a no-op.
            expanded.append([binding[0], binding[1], bridge, 'input',
                             binding[4], None,
                             master_sig, master_sig])
            expanded.append([bridge, 'output', binding[2], binding[3],
                             None, binding[5],
                             slave_sig, slave_sig])

            # Propagate the master's clock domain to the bridge so it has a
            # valid ClockEngine to schedule its FSM event on.
            clock_src = clock_by_comp.get(binding[0])
            if clock_src is not None:
                new_clock_bindings.append([clock_src[0], clock_src[1], bridge, 'clock',
                                           None, None, None, None])

        # Clock-domain bridge pass — runs after the signature-bridge pass so
        # that an already-inserted signature bridge is itself eligible for
        # the splice if it ends up straddling a clock boundary. For every
        # IoV2 binding whose endpoints resolve to different clock sources,
        # splice in the selected clock-bridge component (default
        # 'sync_only', matching v1's transparent auto-crossing).
        _io_v2_sigs = (IoV2BigPacket, IoV2Sync, IoV2Beat)

        def _is_io_v2_sig(sig):
            # Class-based v2 signatures, plus the legacy 'io_v2' string,
            # which is the historic big-packet default (see gvsoc.signature)
            # and must get the same auto-crossing treatment.
            return isinstance(sig, _io_v2_sigs) or sig == IoV2BigPacket.tag

        after_clock = []
        for binding in expanded:
            master_sig = binding[6] if len(binding) > 6 else None
            slave_sig = binding[7] if len(binding) > 7 else None
            if not (_is_io_v2_sig(master_sig) and _is_io_v2_sig(slave_sig)):
                after_clock.append(binding)
                continue

            master_comp = binding[0]
            slave_comp = binding[2]
            master_clock = master_comp._resolve_clock_source()
            slave_clock = slave_comp._resolve_clock_source()
            if master_clock is None or slave_clock is None:
                after_clock.append(binding)
                continue
            if master_clock[0] is slave_clock[0]:
                after_clock.append(binding)
                continue

            override = binding[8] if len(binding) > 8 else None
            if override is not None:
                if isinstance(override, tuple):
                    kind, opts = override
                else:
                    kind, opts = override, {}
            else:
                policy = self.get_clock_bridge_policy(master_clock[0], slave_clock[0])
                if policy is not None:
                    kind, opts = policy
                else:
                    kind, opts = 'sync_only', {}

            bridge_name = f'{binding[0].name}_{binding[1]}_clkbridge'
            bridge = clock_bridges.make(kind, parent=self, name=bridge_name,
                signature=master_sig, **opts)

            after_clock.append([binding[0], binding[1], bridge, 'input',
                                binding[4], None,
                                master_sig, master_sig])
            after_clock.append([bridge, 'output', binding[2], binding[3],
                                None, binding[5],
                                slave_sig, slave_sig])
            new_clock_bindings.append([master_clock[0], master_clock[1], bridge, 'clock',
                                       None, None, None, None])

        self.bindings = after_clock + new_clock_bindings

    def finalize(self):
        pass

    def __finalize(self):
        for component in self.components.values():
            component.__finalize()

        self.finalize()

        self.finalize_done = True

    def get_target(self):

        if self.parent is not None:
            return self.parent.get_target()

        return self

    def get_abspath(self, relpath: str) -> str:
        if self.parent is not None:
            return self.parent.get_abspath(relpath)

        return None

    def _dump_tree_properties(self, tree):
        if len(self.properties) > 0:
            table = rich.table.Table(title=f'[yellow]Properties[/]', title_justify="left")
            table.add_column('Name')
            table.add_column('Value')

            for name, prop in self.properties.items():
                table.add_row(name, str(prop))

            tree.add(table)

    def _process_has_tree_property(self):
        return len(self.properties) > 0

    def declare_user_property(self, name, value, description, cast=None, dump_format=None, allowed_values=None):
        return TargetParameter(
            self, name=name, value=value,
            dump_format=dump_format, cast=cast, description=description, allowed_values=allowed_values
        ).get_value()
