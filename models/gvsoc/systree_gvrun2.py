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

from dataclasses import fields
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
import rich.table
from gvrun.parameter import TargetParameter


generated_components = {}
generated_target_files = {}


class GeneratedComponent(object):

    def __init__(self, name, comp_name, sources, cflags):
        self.sources = sources
        self.cflags = cflags
        self.config_name = name
        self.name = f'gen_{comp_name}'
        self.comp_name = comp_name
        self.full_name = name



# This takes care of computing if a new module has to be compiled for the specified set
# of sources and cflags, so that we can reuse one module for several instances
# This is only for components which are fully handled from Python without cmake
def get_generated_component(sources, cflags):
    # Compute full name what sources and cflags
    name = ''.join(sources + cflags)
    # Compute a unique hash so that we don't compile with long name
    name_hash = int(hashlib.md5(name.encode('utf-8')).hexdigest()[0:7], 16)

    # Loop until we find a hash which does not collide with existing components
    while True:
        # We also put first source code in the name so that the user can understand a bit
        # what is being compiled.
        # Remove characters which prevent cmake from compiling
        template_name = sources[0].replace('/', '_').replace('.', '_')
        comp_name = f'{template_name}_{name_hash}'

        # Check if this collides
        generated_component = generated_components.get(comp_name)
        if generated_component is None or generated_component.full_name == name:
            break

        name_hash += 1

    # If the component is not yet registered do it
    if generated_components.get(comp_name) is None:
        generated_components[comp_name] = GeneratedComponent(name=name, comp_name=comp_name, sources=sources, cflags=cflags)

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
    signature: str
        The signature of the interface. If specified, this is used to check that the interface
        is bound to a master interface of the same signature.
    """
    def __init__(self, component: 'Component', itf_name: str, signature: str=None):
        self.component = component
        self.itf_name = itf_name
        self.signature = signature


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

    def __init__(self, parent: 'Component', name: str, tree=None, options=None):
        super().__init__(name, parent=parent, tree=tree)
        self.parent = parent
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

        if parent is not None and isinstance(parent, Component):
            parent.__add_component(name, self)

        if tree is not None:
            for f in fields(tree):
                value = getattr(tree, f.name)
                if isinstance(value, (int, bool, str)):
                    self.add_property(f.name, value)


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
        """
        self.sources += sources

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

    def itf_bind(self, master_itf_name: str, slave_itf: SlaveItf, signature: str=None,
            composite_bind: bool=False):
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
        signature : str
            The signature of the interface. If specified, this is used to check that the interface
            is bound to a slave interface of the same signature.
        """

        if signature is not None and slave_itf.signature is not None and \
                signature != slave_itf.signature:
            master_name = f'{signature}@{self.get_path()}->{master_itf_name}'
            slave_name = f'{slave_itf.signature}@{slave_itf.component.get_path()}->{slave_itf.itf_name}'
            raise RuntimeError(f'Invalid signature (master: {master_name}, slave: {slave_name})')

        if composite_bind:
            self.bind(self, master_itf_name, slave_itf.component, slave_itf.itf_name)
        else:
            self.parent.bind(self, master_itf_name, slave_itf.component, slave_itf.itf_name)

    def regmap_gen(self, template, outdir, name, block=None, headers=['regfields', 'gvsoc']):
        # Only generate the archi file once for all instances
        global generated_target_files
        full_name = name + '_'.join(headers)
        if generated_target_files.get(full_name) is None:
            import regmap.regmap
            import regmap.regmap_md
            import regmap.regmap_c_header
            generated_target_files[full_name] = True
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


    def bind(self, master, master_itf, slave, slave_itf, master_properties=None, slave_properties=None):
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
        """
        self.bindings.append([master, master_itf, slave, slave_itf, master_properties, slave_properties])

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

    def get_config(self):
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
            config[component_name] = component.get_config()

        config.update(self.properties)

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

        for component in self.components.values():
            component.gen_gui_stub(parent_signal)

    def gen_gui(self, parent_signal):
        name = self.get_name()
        if name is not None:
            # By default, we always generate a combiner but we could do it only if a dedicated
            # option is there
            # signal = gvsoc.gui.Signal(self, parent_signal, name=name, skip_if_no_child=True)
            # return signal
            return gvsoc.gui.SignalGenFromSignals(self, parent_signal, to_signal=self.name,
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

        for interface in self.interfaces:
            self.bindings.append([interface.comp, interface.name, interface.remote_itf.comp,
                interface.remote_itf.name, interface.properties, interface.remote_itf.properties])

        for component in self.components.values():
            component.__build()

        if len(self.bindings) != 0:
            for binding in self.bindings:
                master_port = binding[0].__add_port(binding[1])
                slave_port = binding[2].__add_port(binding[3])
                master_port.bind(slave_port, master_properties=binding[4], slave_properties=binding[5])

        self.build_done = True

        if self.component is None and len(self.sources) != 0:
            self.generated_component = get_generated_component(self.sources, self.c_flags)
            self.add_property('vp_component', self.generated_component.name)

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
