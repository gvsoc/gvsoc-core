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


import json_tools as js
import collections
import os
import json
import sys
import shutil

# This is terribly slow to import, find a way to import them only when needed
# import regmap.regmap as rmap
# import regmap.regmap_md_mistune as regmap_md_mistune
# import regmap.regmap_c_header as regmap_c_header


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


class Component(object):
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

    def __init__(self, parent, name, options=[], is_top=False):
        self.name = name
        self.parent = parent
        self.json_config_files = []
        self.components = {}
        self.properties = {}
        self.bindings = []
        self.ports = {}
        self.build_done = False
        self.finalize_done = False
        self.options = []
        self.comp_options = {}
        self.is_top = is_top
        self.vcd_group_create = True
        self.vcd_group_closed = True
        self.component = None
        self.interfaces = []

        if len(options) > 0:
            options_list = []

            for option in options:
                name, value = option.split('=', 1)
                name_list = name.split('/')
                name_list.append(value)

                options_list.append(name_list)
            
            self.__set_options(options_list)

        if parent is not None:
            parent.add_component(name, self)

    def gen_stimuli(self):
        """Generate stimuli.

        This method can be called to make the system described by this component hierarchy
        generates all the needed stimuli to launch execution.
        ANy sub-component can overload this method to get called and generates stimuli.

        Parameters
        ----------
        work_dir : str
            Working directory where the stimuli should be generated
        """
        for component in self.components.values():
            component.gen_stimuli()


    def get_path(self, child_path=None, gv_path=False, *kargs, **kwargs):
        """Get component path.

        This returns the full path of the component, taking into account all the parents.

        Parameters
        ----------
        child_path : str, optional
            The path of the childs which can should appended to this path.
        *kargs, **kwargs
            Additional arguments which can be passed to the parents.

        Returns
        -------
        str
            The path.
        """

        if not gv_path or not self.is_top:
            path = self.name
            if child_path is not None:
                path = self.name + '/' + child_path

            if self.parent is not None:
                path = self.parent.get_path(child_path=path, gv_path=gv_path, *kargs, **kwargs)
        
        else:
            path = child_path

        return path


    def register_flash(self, flash):
        if self.parent is not None:
            self.parent.register_flash(flash)

    def declare_flash(self, path=None):
        """Declare a flash.

        This declaration is propagated towards the parents so that the runner taking care of
        the flash image generation knows where the flash are.

        Parameters
        ----------
        path : str, optional
            This should be None when the flash is triggering the declaration and is then used
            to construct the path of the flash when going through the parents.
        """
        if self.parent is not None:
            if path is None:
                self.parent.declare_flash(self.name)
            else:
                self.parent.declare_flash(self.name + '/' + path)


    def declare_runner_target(self, path=None):
        """Declare a runner target.

        A runner target is a stand-alone system like a board for which the runner should generate stimuli like
        a flash image. This declaration is propagated towards the parents so that the runner taking care of
        the system knows where are the targets, and where it should generates stimulis.

        Parameters
        ----------
        path : str, optional
            This should be None when the runner target is triggering the declaration and is then used
            to construct the path of the runner when going through the parents.
        """
        if self.parent is not None:
            if path is None:
                self.parent.declare_runner_target(self.name)
            else:
                self.parent.declare_runner_target(self.name + '/' + path)


    def add_component(self, name, component):
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

        # Determine the set of options which should be propagated to the sub-component
        # based on the path
        comp_options = []
        for option in self.options:
            option_name = None
            name_pos = 0
            for item in option:
                if item != "*" and item != "**":
                    option_name = item
                    break
                name_pos += 1

            if option_name == name:
                comp_options.append(option[name_pos + 1:])
            elif option[0] == "*":
                comp_options.append(option[1:])
            elif option[0] == "**":
                comp_options.append(option)

        if len(comp_options) != 0:

            component.__set_options(comp_options)

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
        return self.components[name]


    def add_property(self, name, property, format=None):
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

        comp_options = self.comp_options

        for item in name.split('/'):
            if comp_options.get(item) is None:
                option_property = None
                break
            else:
                option_property = comp_options.get(item)
            comp_options = comp_options.get(item)

        property = None
        for item in name.split('/'):
            if property is None:
                property = self.properties.get(item)
            else:
                property = property.get(item)

        if option_property is not None:
            if property is not None and type(property) == list:
                if type(property) == list:
                    property = property.copy()
                    if type(option_property) == list:
                        property += option_property
                    else:
                        property.append(option_property)
            else:
                if not isinstance(property, type(option_property)):
                    property = self.__convert(option_property, property)
                else:
                    property = option_property


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

    def get_component_list(self):
        result = []
        
        if self.component is not None:
            result.append(self.component)

        def Union(lst1, lst2):
            final_list = list(set(lst1) | set(lst2))
            return final_list

        for child in self.components.values():
            result = Union(result, child.get_component_list())

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

        for json_config_file in self.json_config_files:
            config = self.__merge_properties(config, js.import_config_from_file(json_config_file, find=True, interpret=True, gen=True).get_dict())

        for component_name, component in self.components.items():
            component_config = { component_name: component.get_config() }
            config = self.__merge_properties(config, component_config)

        #config = self.merge_options(config, self.comp_options, self.properties)

        config = self.__merge_properties(config, self.properties, self.comp_options)

        if len(self.bindings) != 0:
            if config.get('bindings') is None:
                config['bindings'] = []
            for binding in self.bindings:
                master_name = 'self' if binding[0] == self else binding[0].name
                slave_name = 'self' if binding[2] == self else binding[2].name
                config['bindings'].append([ '%s->%s' % (master_name, binding[1]), '%s->%s' % (slave_name, binding[3])])

        if len(self.components.values()) != 0:
            config = self.__merge_properties(config, { 'components' : list(self.components.keys()) })

        if len(self.ports) != 0:
            config = self.__merge_properties(config, { 'ports' : list(self.ports.keys()) })

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


    def add_properties(self, properties):
        self.properties = self.__merge_properties(self.properties, properties)

    def vcd_group(self, closed=True, skip=False):
        self.vcd_group_create = not skip
        self.vcd_group_closed = closed

    def gen_gtkw_tree(self, tree, traces, filter_traces=False):
        
        if filter_traces:
            comp_traces = []
            subcomps_traces = []

            for trace in traces:
                if trace[0][0] == self.name:
                    if len(trace[0][1:]) == 0:
                        comp_traces.append([trace[0][1:], trace[1], trace[2]])
                    subcomps_traces.append([trace[0][1:], trace[1], trace[2]])
        else:
            comp_traces = []
            subcomps_traces = traces

        self.gen_gtkw_conf(tree, comp_traces)

        if self.vcd_group_create or tree.gen_full_tree:
            tree.begin_group(self.name, closed=self.vcd_group_closed)

        self.__gen_gtkw_wrapper(tree, comp_traces)

        for component in self.components.values():

            component.gen_gtkw_tree(tree, traces=subcomps_traces, filter_traces=self.is_top or filter_traces)

        if self.vcd_group_create or tree.gen_full_tree:
            tree.end_group(self.name, closed=self.vcd_group_closed)

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

    def __set_options(self, options):
        for option in options:

            if True:
                comp_options = self.comp_options

                for item in option[:-2]:

                    if item in ['*', '**']:
                        continue

                    if comp_options.get(item) is None:
                        comp_options[item] = {}

                    comp_options = comp_options.get(item)

                name = option[-2]
                value = option[-1]

                if type(value) == dict or type(value) == collections.OrderedDict():
                    value = value.copy()

                if comp_options.get(name) is None:
                    comp_options[name] = value
                elif type(comp_options.get(name)) == list:
                    comp_options[name].append(value)
                else:
                    comp_options[name] = [ comp_options[name], value ]

            else:

                if len(option) == 1:
                    name = option[-2]
                    value = option[-1]
                    self.comp_options[name] = value

        self.options = options

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

    def finalize(self):
        pass

    def __finalize(self):
        for component in self.components.values():
            component.__finalize()

        self.finalize()

        self.finalize_done = True

    def __merge_properties(self, dst, src, options=None, is_root=True):

        if type(src) == dict or type(src) == collections.OrderedDict:

            # Copy the source dictionary so that we can append the options
            # which do not appears in src
            src_merged = src.copy()

            # Copy all the options which do not appear in src
            if options is not None:
                for name, value in options.items():
                    if src_merged.get(name) is None and not is_root:
                        src_merged[name] = value

            # And merge dst, src and options
            for name, value in src_merged.items():
                if dst.get(name) is None:
                    new_dst = {}
                else:
                    new_dst = dst.get(name)

                if options is not None:
                    new_options = options.get(name)
                else:
                    new_options = None

                dst[name] = self.__merge_properties(new_dst, value, new_options, is_root=False)

            return dst

        elif type(src) == list:
            result = src.copy()
            if options is not None:
                if type(options) == list:
                    result += options
                else:
                    result.append(options)

            return result

        else:
            if options is not None:
                return self.__convert(options, src)
            else:
                return src

    def __convert(self, value, dest):
        if isinstance(dest, bool):
            if isinstance(value, str):
                return value == 'true' or value == 'True'
            else:
                return bool(value)
        elif isinstance(dest, int):
            if isinstance(value, str):
                return int(value, 0)
            else:
                return int(value)
        else:
            return value


    def get_target(self):

        if self.parent is not None:
            return self.parent.get_target()

        return None

    
    def regmap(self, copy=False, gen=False):

        for component in self.components.values():
            component.regmap(copy, gen)

        spec = self.get_property('regmap/spec')

        if copy:
            spec_src = self.get_property('regmap/spec_src')

            if spec_src is not None and spec is not None:
                print (f'Copying {spec_src} to {spec}')
                shutil.copy(spec_src, spec)

        if gen:
            header_dir = self.get_property('regmap/header_prefix')
            name = self.get_property('regmap/name')
            headers = self.get_property('regmap/headers')

            if spec is not None and header_dir is not None and name is not None:
                print(f'Generating archi files {headers} to {header_dir}')

                regmap = rmap.Regmap(name)
                regmap_md_mistune.import_md(regmap, spec)
                regmap_c_header.dump_to_header(regmap=regmap, name=name, header_path=header_dir, headers=headers)