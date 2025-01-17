#
# Copyright (C) 2021 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
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

import socket
import threading
import socket
import os




class Proxy(object):
    """
    A class used to control GVSOC through the socket proxy

    :param host: str,
        a string giving the hostname where the proxy is running
    :param port: int,
        the port where to connect
    """

    class _Socket_proxy_reader_thread(threading.Thread):

        def __init__(self, socket):
            super(Proxy._Socket_proxy_reader_thread, self).__init__()
            self.socket = socket
            self.lock = threading.Lock()
            self.condition = threading.Condition(self.lock)
            self.replies = {}
            self.matches = {}
            self.payloads = {}
            self.running = False
            self.timestamp = 0
            self.exit_callback = None
            self.sim_has_exited = False
            self.sim_exit_code = -1
            self.syscall_stop = False

        def __quit(self, status):
            self.lock.acquire()
            self.sim_has_exited = True
            self.sim_exit_code = status
            self.condition.notify_all()
            if self.exit_callback is not None:
                self.lock.release()
                self.exit_callback[0](status, *self.exit_callback[1], **self.exit_callback[2])
            else:
                self.lock.release()
            # os._exit(status)
            # exit(status)

        def run(self):
            while True:
                reply = ""
                try:
                    while True:
                        byte = self.socket.recv(1).decode('utf-8')
                        # if not byte:
                        #     self.__quit(-1)
                        #     return
                        reply += byte
                        if byte == '\n':
                            break
                except:
                    # self.__quit(-1)
                    return

                req = None
                is_stop = None
                is_run = None
                msg = ""
                err = None
                err_msg = None

                for arg in reply.split(';'):
                    name, value = arg.split('=', 1)
                    if name == 'req':
                        req = int(value)
                    elif name == 'exit':
                        self.__quit(int(value))
                    elif name == 'msg':
                        msg = value
                        if msg.find('syscall_stop') == 0:
                            self.syscall_stop = True
                        elif msg.find('stopped') == 0:
                            is_stop = int(value.split('=')[1])
                        elif msg.find('running') == 0:
                            is_run = int(value.split('=')[1])

                    elif name == 'err':
                        err = value

                    elif name == 'err_msg':
                        err_msg = value

                    elif name == 'payload':
                        callback = self.matches.get('%s' % req)
                        if callback is not None:
                            callback[0](*callback[1], **callback[2])

                        else:

                            payload = bytearray()
                            size = int(value)
                            while len(payload) < size:
                                response = self.socket.recv(size - len(payload))
                                payload += response

                            self.lock.acquire()
                            self.payloads[req] = payload
                            self.condition.notify_all()
                            self.lock.release()

                if req is None:
                    raise RuntimeError('Unknown reply: ' + req)

                self.lock.acquire()

                if is_stop is not None:
                    self.timestamp = is_stop
                    self.running = False
                elif is_run is not None:
                    self.running = True

                self.replies[req] = msg
                self.condition.notify_all()
                self.lock.release()

        def _get_payload(self, req):
            self.lock.acquire()
            while self.payloads.get(req) is None:
                self.condition.wait()
            payload = self.payloads[req]
            del self.payloads[req]
            self.lock.release()

            return payload



        def wait_reply(self, req):

            self.lock.acquire()
            while self.replies.get(req) is None:
                self.condition.wait()
            reply = self.replies[req]
            del self.replies[req]

            self.lock.release()

            return reply

        def wait_stopped(self):
            self.lock.acquire()
            while not self.syscall_stop:
                self.condition.wait()
            self.syscall_stop = False
            self.lock.release()

        def wait_running(self):
            self.lock.acquire()
            while not self.running:
                self.condition.wait()
            self.lock.release()


        def register_callback(self, req, callback, *kargs, **kwargs):
            match = '%s' % req
            self.lock.acquire()
            self.matches[match] = callback, kargs, kwargs
            self.lock.release()

        def unregister_callback(self, req):
            match = '%s' % req
            self.lock.acquire()
            self.matches[match] = None
            self.lock.release()

        def handle_err(self, error, error_str=None):
            if error != 0:
                if error_str is None:
                    raise RuntimeError("Proxy command failed with status %s" % error)
                else:
                    raise RuntimeError("Proxy command failed with message: %s" % error_str)

        def register_exit_callback(self, callback, *kargs, **kwargs):
            self.lock.acquire()
            self.exit_callback = [ callback, kargs, kwargs ]
            self.lock.release()


    def __init__(self, host: str = 'localhost', port: int = 42951, cosim: bool=False):
        self.req_id = 0
        self.cosim = cosim

        self.lock = threading.Lock()

        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((host, port))

        self.reader = self._Socket_proxy_reader_thread(self.socket)
        self.reader.start()

    def _get_req(self):
        self.lock.acquire()
        req = self.req_id
        self.req_id += 1
        self.lock.release()

        return req

    def _send_cmd(self, cmd, wait_reply=True, keep_lock=False):
        self.lock.acquire()
        req = self.req_id
        self.req_id += 1
        self.socket.send(('req=%d;cmd=%s\n' % (req, cmd)).encode('ascii'))

        if not keep_lock:
            self.lock.release()

        if wait_reply:
            return self.reader.wait_reply(req)
        else:
            return req

    def _unlock_cmd(self):
        self.lock.release()

    def wait_stop(self):
        """Wait until execution stops.

        This will block the caller until gvsoc stops execution.

        """
        self.reader.wait_stopped()

    def wait_running(self):
        """Wait until GVSOC is running.

        This will block the caller until gvsoc starts execution.

        """
        self.reader.wait_running()

    def stop(self):
        """Stop execution.
        """
        self._send_cmd('stop')

    def close(self):
        """Close the proxy.

        This will free resources and close threads so that simulation can properly exit.
        """
        self.socket.shutdown(socket.SHUT_RDWR)
        self.socket.close()
        self.reader.join()


    def trace_add(self, trace: str):
        """Enable a trace.

        :param trace: A regular expression used to enable traces
        """

        self._send_cmd('trace add %s' % trace)

    def trace_remove(self, trace: str):
        """Disable a trace.

        :param trace: A regular expression used to disable traces
        """

        self._send_cmd('trace remove %s' % trace)

    def trace_level(self, level: str):
        """Changes the trace level.

        :param level: The trace level, can be "error", "warning", "info", "debug" or "trace"
        """

        self._send_cmd('trace level %s' % level)

    def event_add(self, event: str):
        """Enable an event.

        :param event: A regular expression used to enable events
        """

        self._send_cmd('event add %s' % event)

    def event_remove(self, event: str):
        """Disable a trace.

        :param event: A regular expression used to enable events
        """

        self._send_cmd('event remove %s' % event)

    def run(self, duration: int = None):
        """Starts execution.

        :param duration: Specify the duration of the execution in picoseconds (will execute forever by default)
        """

        if duration is not None:
            self._send_cmd('step %d' % (duration))
        else:
            self._send_cmd('run')

    def release(self):
        pass

    def retain(self):
        pass

    def is_sim_finished(self):
        return self.reader.sim_has_exited

    def wait_sim_finished(self):
        self.reader.lock.acquire()

        while not self.reader.sim_has_exited:
            self.reader.condition.wait()

        self.reader.lock.release()

    def join(self):
        """Wait end of simulation.

        Blocks until the simulation exit and return the simulation return value

        :return The simulation return value
        """
        self.wait_sim_finished()

        return self.reader.sim_exit_code


    def quit(self, status: int = 0):
        """Exit simulation.

        :param status: Specify the status value.
        """

        self._send_cmd('quit %d' % status)

    def terminate(self):
        self._send_cmd('terminate')


    def _get_component(self, path):
            result = self._send_cmd('get_component %s' % path)

            return result.replace('\n', '')


    def register_exit_callback(self, callback, *kargs, **kwargs):
        """Register exit callback

        The callback is called when GVSOC exits. If no callback is registered,
        os._exit is called when GVSOC exits.

        :param callback: The function to be called when GVSOC exits
        :param kargs: Arguments propagated to the callback
        :param kwargs: Arguments propagated to the callback
        """
        self.reader.register_exit_callback(callback, *kargs, **kwargs)


class Router(object):
    """
    A class used to inject memory accesses into a router

    :param proxy: The proxy object. This class will use it to send command to GVSOC through the proxy connection.
    :param path: The path to the router in the architecture.
    """

    def __init__(self, proxy: Proxy, path: str = '**/chip/soc/axi_ico'):
        self.proxy = proxy
        self.component = proxy._get_component(path)



    def mem_write(self, addr: int, size: int, values: bytes):
        """Inject a memory write.

        The access is generated by the router where this class is connected and is
        injected as a debug request to not disturb the timing.

        :param addr: The address of the access.
        :param size: The size of the access in bytes.
        :param values: The sequence of bytes to be written, in little endian byte ordering.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        cmd = 'component %s mem_write 0x%x 0x%x' % (self.component, addr, size)

        # Since we need to send a command and right after the data,
        # we have to keep the command queue locked to avoid mixing our data
        # with another command
        req = self.proxy._send_cmd(cmd, keep_lock=True, wait_reply=False)
        self.proxy.socket.send(values)
        self.proxy._unlock_cmd()

        self.proxy.reader.wait_reply(req)

    def mem_read(self, addr: int, size: int) -> bytes:
        """Inject a memory read.

        The access is generated by the router where this class is connected and is
        injected as a debug request to not disturb the timing.

        :param addr: int, The address of the access.
        :param size: int, The size of the access in bytes.

        :return: bytes, The sequence of bytes read, in little endian byte ordering.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """

        # Since we need to send a command and right after we receive the data,
        # we have to keep the command queue locked to avoid mixing our data
        # with another command
        self.read_size = size
        cmd = 'component %s mem_read 0x%x 0x%x' % (self.component, addr, size)
        req = self.proxy._send_cmd(cmd, keep_lock=True, wait_reply=False)

        reply = self.proxy.reader._get_payload(req)

        self.proxy._unlock_cmd()

        self.proxy.reader.wait_reply(req)

        return reply


    def mem_write_int(self, addr: int, size: int, value: int):
        """Write an integer.

        The access is generated by the router where this class is connected and is
        injected as a debug request to not disturb the timing.

        :param addr: int, The address of the access.
        :param size: int, The size of the access in bytes.
        :param value: int, The integer to be written.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        return self.mem_write(addr, size, value.to_bytes(size, byteorder='little'))

    def mem_read_int(self, addr: int, size: int) -> int:
        """Read an integer.

        The access is generated by the router where this class is connected and is
        injected as a debug request to not disturb the timing.

        :param addr: int, The address of the access.
        :param size: int, The size of the access in bytes.

        :return: int, The integer read.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        values = self.mem_read(addr, size)
        return int.from_bytes(values, byteorder='little')




class Ssm6515(object):
    """
    A class used to control ssm6515 dac

    :param proxy: The proxy object. This class will use it to send command to GVSOC through the proxy connection.
    :param path: The path to the dac in the architecture.
    """

    def __init__(self, proxy: Proxy, path: str = '**/chip/soc/axi_ico'):
        self.proxy = proxy
        self.component = proxy._get_component(path)


    def pdm_mode_configure(self, output_pcm_rate : int = None, output_pcm_sample_width : int = None, filepath : str = None):
        """Set the dac pdm configuration.

        :param filepath: The path to the produced output file.
        :param output_pcm_sample_width: Desired output sample width (in output file).
        :param output_pcm_rate: Desired output pcm samplerate.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        option = ''
        if(filepath):
            option += ' file_path %s' % (filepath)
        if(output_pcm_sample_width):
            option += ' width %d' % (output_pcm_sample_width)
        if(output_pcm_rate):
            option += ' pcm_freq %d' % (output_pcm_rate)
        cmd = 'component %s mode pdm %s' % (self.component, option)

        req = self.proxy._send_cmd(cmd)


    def i2s_mode_configure(self, pcm_rate : int = None, slot_width : int = None, filepath : str = None):
        """Set the dac i2s configuration.

        :param filepath: The path to the produced output file.
        :param slot_width: Slot width in i2s cnfiguration.
        :param pcm_rate: Desired output pcm samplerate.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        option = ''
        if(filepath):
            option += ' file_path %s' % (filepath)
        if(slot_width):
            option += ' width %d' % (slot_width)
        if(pcm_rate):
            option += ' pcm_freq %d' % (pcm_rate)
        cmd = 'component %s mode i2s %s' % (self.component, option)

        req = self.proxy._send_cmd(cmd)


    def debug_enabled(self, debug: bool = True):
        """Enable debug mode of the ssm

        Will dump into a file the audio signal taken by the mic at the reception (just after pdm demod if necessary).
        The debug file is named "[dac_name]_debug.wav".
        """
        cmd = 'component %s debug %d' % (self.component, debug)

        req = self.proxy._send_cmd(cmd)


class TransferFunction(object):
    """
    A class used to control input of TF

    :param proxy: The proxy object. This class will use it to send command to GVSOC through the proxy connection.
    :param path: The path to the TF in the architecture.
    """

    def __init__(self, proxy: Proxy, path: str = '**/chip/soc/axi_ico'):
        self.proxy = proxy
        self.component = proxy._get_component(path)


    def configure(self, filter: list, frequency : int = None, debug_files : bool = False):
        """Configure the transfert function.

        Filter : give the parameters of the TF.
        Frequency : give the PCM frequency at witch Transfert Function should run.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        # Put filter into a file
        filter_path = self.component + "filter_params.txt"

        # Écriture de la liste dans le fichier
        with open(filter_path, 'w') as file_filt:
            for element in filter:
                file_filt.write(str(element) + '\n')

        option = ''
        if(debug_files):
            option += ' debug_files %d' % (debug_files)
        if(frequency):
            option += ' rate %d' % (frequency)
        cmd = 'component %s %s filter %s' % (self.component, option, filter_path)

        req = self.proxy._send_cmd(cmd)


class Pdm_microphone(object):
    """
    A class used to control input of microphone

    :param proxy: The proxy object. This class will use it to send command to GVSOC through the proxy connection.
    :param path: The path to the mic in the architecture.
    """

    def __init__(self, proxy: Proxy, path: str = '**/chip/soc/axi_ico'):
        self.proxy = proxy
        self.component = proxy._get_component(path)


    def set_generated_sine(self, freq: int, amplitude: float):
        """Inject a memory write.

        The access is generated by the router where this class is connected and is
        injected as a debug request to not disturb the timing.

        :param freq: The frequency of the sinusoide.
        :param amplitude: The amplitude of the sinusoide : 1 is full scale.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        cmd = 'component %s set_generated_sine frequency %d amplitude %.5f' % (self.component, freq, amplitude)

        req = self.proxy._send_cmd(cmd)


    def debug_enabled(self, debug: bool = True):
        """Enable debug mode of the mic

        Will dump into a file the audio signal taken by the mic just before the pdm modulation.
        The debug file is named "[mic_name]_debug.wav".
        """
        cmd = 'component %s debug %d' % (self.component, debug)

        req = self.proxy._send_cmd(cmd)


    def set_input_file(self, path: str):
        """Inject an input file to the mic.
        The pdm mic will load the file and modulate it to a PDM signal.
        """
        cmd = 'component %s set_input_file %s' % (self.component, path)

        req = self.proxy._send_cmd(cmd)


class Testbench(object):
    """Testbench class.

    This class can be instantiated to get access to the testbench.

    :param proxy: Proxy, The proxy object. This class will use it to send command to GVSOC through the proxy connection.
    :param path: string, optional, The path to the testbench in the architecture.
    """

    def __init__(self, proxy: Proxy, path: str = '**/testbench/testbench'):
        self.proxy = proxy
        self.component = proxy._get_component(path)


    def i2s_get(self, id: int = 0):
        """Open an SAI.

        Open an SAI and return an object which can be used to interact with it.

        :param id: int, optional, The SAI identifier.

        :return: Testbench_i2s, An object which can be used to access the specified SAI.
        """
        return Testbench_i2s(self.proxy, self.component, id)


    def spi_get(self, id: int = 0, cs = 0):
        """Open a spi interface.

        Open a spi interface and return an object which can be used to interact with it.

        :param id: int, optional, The spi interface identifier.
        :param cs: int, optional, The spi cs interface identifier.

        :return: Testbench_spi, An object which can be used to access the specified uart interface.
        """
        return Testbench_spi(self.proxy, self.component, id, cs)


    def uart_get(self, id: int = 0):
        """Open a uart interface.

        Open a uart interface and return an object which can be used to interact with it.

        :param id: int, optional, The uart interface identifier.

        :return: Testbench_uart, An object which can be used to access the specified uart interface.
        """
        return Testbench_uart(self.proxy, self.component, id)


class Testbench_uart(object):
    """Class instantiated for each manipulated uart interface.

    It can used to interact with the uart interface, like injecting streams.

    :param proxy: Proxy, The proxy object. This class will use it to send command to GVSOC through the proxy connection.
    :param testbench: int, The testbench object.
    :param id: int, optional, The identifier of the uart interface.
    """

    def __init__(self, proxy: Proxy, testbench: Testbench, id=0):
        self.id = id
        self.proxy = proxy
        self.testbench = testbench
        self.callback = None
        self.lock = threading.Lock()
        self.condition = threading.Condition(self.lock)
        self.pending_rx_bytes = bytearray()
        self.req = None

    def open(self, baudrate: int, word_size: int=8, stop_bits: int=1, parity_mode: bool=False, ctrl_flow: bool=True,
            is_usart: bool=False, usart_polarity: int=0, usart_phase: int=0):
        """Open and configure a uart interface.

        :param baudrate: int, Specify the uart baudrate in bps
        :param word_size: int, optional, Specify the size in bits of the uart bytes.
        :param stop_bits: int, optional, Specify the number of stop bits.
        :param parity_mode: bool, optional, True if parity is enabled.
        :param ctrl_flow: bool, optional, True if control flow is enabled.
        :param is_usart: bool, optional, True if uart is in usart mode.
        :param usart_polarity: int, optional, Usart polarity.
        :param usart_phase: int, optional, Usart phase.

        :raises: RuntimeError, if there is any invalid parameter.
        """

        options = ''
        options += ' itf=%d' % self.id
        options += ' enabled=1'
        options += ' baudrate=%d' % baudrate
        options += ' word_size=%d' % word_size
        options += ' stop_bits=%d' % stop_bits
        options += ' parity_mode=%d' % parity_mode
        options += ' ctrl_flow=%d' % ctrl_flow
        options += ' is_usart=%d' % is_usart
        options += ' usart_polarity=%d' % usart_polarity
        options += ' usart_phase=%d' % usart_phase
        cmd = 'component %s uart setup %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def close(self):
        """Close the uart interface.

        :raises: RuntimeError, if there is any error while closing.
        """
        options = ''
        options += ' itf=%d' % self.id
        options += ' enabled=0'
        cmd = 'component %s uart setup %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def tx(self, values: bytes):
        """Send data to the uart.

        This enqueues an array of bytes to be transmitted. If previous transfers are not finished,
        these bytes will be transfered after.

        :param values: bytes, The sequence of bytes to be sent, in little endian byte ordering.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        cmd = 'component %s uart tx %d %d' % (self.testbench, self.id, len(values))

        # Since we need to send a command and right after the data,
        # we have to keep the command queue locked to avoid mixing our data
        # with another command
        req = self.proxy._send_cmd(cmd, keep_lock=True, wait_reply=False)
        self.proxy.socket.send(values)
        self.proxy._unlock_cmd()

        self.proxy.reader.wait_reply(req)

    def rx(self, size=None):
        """Read data from the uart.

        Once reception on the uart is enabled, the received bytes are pushed to a fifo. This method
        can be called to pop received bytes from the FIFO.

        :param size: int, The number of bytes to be read. If it is None, it returns the bytes which has already been received.

        :return: bytes, The sequence of bytes received, in little endian byte ordering.

        :raises: RuntimeError, If the access generates an error in the architecture.
        """
        self.lock.acquire()

        if size is not None:
            while len(self.pending_rx_bytes) < size and self.rx_enabled:
                self.condition.wait()

            if not self.rx_enabled:
                self.lock.release()
                return None
        else:
            size = len(self.pending_rx_bytes)

        reply = self.pending_rx_bytes[0:size]
        self.pending_rx_bytes = self.pending_rx_bytes[size:]

        self.lock.release()

        return reply


    def rx_enable(self):
        """Enable receiving bytes from the uart.

        Any byte received from the uart either triggers the callback execution if it has been registered,
        or is pushed to a FIFO which can read.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        self.req = self.proxy._get_req()
        self.proxy.reader.register_callback(self.req, self.__handle_rx)
        cmd = 'component %s uart rx %d 1 %d' % (self.testbench, self.id, self.req)
        self.proxy._send_cmd(cmd)

        self.rx_enabled = True


    def rx_disable(self):
        """Disable receiving bytes from the uart.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        self.proxy.reader.unregister_callback(self.req)
        cmd = 'component %s uart rx %d 0' % (self.testbench, self.id)
        self.proxy._send_cmd(cmd)

        self.lock.acquire()
        self.rx_enabled = False
        self.condition.notify_all()
        self.lock.release()


    def __handle_rx(self):
        self.lock.acquire()
        reply = self.proxy.socket.recv(1)
        if self.callback is not None:
            self.callback[0](1, reply, *self.callback[1], **self.callback[2])
        else:
            self.pending_rx_bytes += reply
            self.condition.notify()
        self.lock.release()


    def rx_attach_callback(self, callback, *kargs, **kwargs):
        """Attach callback for receiving bytes from the uart.

        All bytes received from the uart now triggers the execution of the specified callback.
        This must be called only when uart reception is disabled.
        The callback will be called asynchronously by a different thread and so special care must be taken
        to access shared variables using locks. Also the proxy can not be used from the callback.

        :param callback: The function to be called when bytes are received from the uart. First parameters will contain
                         number of bytes and received, and second one will be the bytes received.
        :param kargs: Arguments propagated to the callback
        :param kwargs: Arguments propagated to the callback

        :return: bytes, The sequence of bytes received, in little endian byte ordering.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        self.callback = callback, kargs, kwargs


    def rx_detach_callback(self):
        """Detach a callback.

        The callback previously attached won't be called anymore.
        This must be called only when uart reception is disabled.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        self.callback = None

    def flush(self):
        req = self.proxy._get_req()
        cmd = 'component %s uart flush %d %d' % (self.testbench, self.id, req)
        self.proxy._send_cmd(cmd)
        self.proxy.reader.wait_reply(req)

    def clear(self):
        req = self.proxy._get_req()
        cmd = 'component %s uart clear %d %d' % (self.testbench, self.id, req)
        self.proxy._send_cmd(cmd)
        self.proxy.reader.wait_reply(req)

        self.lock.acquire()
        self.pending_rx_bytes = bytearray()
        self.lock.release()


class Testbench_i2s(object):
    """Class instantiated for each manipulated SAI.

    It can used to interact with the SAI, like injecting streams.

    :param proxy: Proxy, The proxy object. This class will use it to send command to GVSOC through the proxy connection.
    :param testbench: int, The testbench object.
    :param id: int, optional, The identifier of the SAI interface.
    """

    def __init__(self, proxy: Proxy, testbench: Testbench, id=0):
        self.id = id
        self.proxy = proxy
        self.testbench = testbench

    def open(self, word_size: int = 16, sampling_freq: int = -1, nb_slots: int = 1, is_pdm: bool = False,
            is_full_duplex: bool = False, is_ext_clk: bool = False, is_ext_ws: bool = False, is_sai0_clk: bool = False,
            is_sai0_ws: bool = False, clk_polarity: int = 0, ws_polarity: int = 0, ws_delay: int = 1):
        """Open and configure SAI.

        :param word_size: int, optional, Specify the frame word size in bits.
        :param sampling_freq: int, optional, Specify the sampling frequency. This is used either to generate the clock when
            it is external or to check that internally generated one is correct.
        :param nb_slots: int, optional,
            Number of slots in the frame.
        :param is_pdm: bool, optional,
            True if the stream is a PDM stream.
        :param is_full_duplex: bool, optional,
            True if the SAI is used in full duplex mode.
        :param is_ext_clk: bool, optional,
            True is the clock is generated by the testbench.
        :param is_ext_ws: bool, optional,
            True is the word strobe is generated by the testbench.
        :param is_sai0_clk: bool, optional,
            True is the the clock should be taken from SAI0.
        :param is_sai0_ws: bool, optional,
            True is the the word strobe should be taken from SAI0.
        :param clk_polarity: int, optional,
            Clock polarity, definition is the same as SAI0 specifications.
        :param ws_polarity: int, optional,
            Word strobe polarity, definition is the same as SAI0 specifications.
        :param ws_delay: int, optional,
            Word strobe delay, definition is the same as SAI0 specifications.

        :raises: RuntimeError, if there is any invalid parameter.
        """

        options = ''
        options += ' itf=%d' % self.id
        options += ' enabled=1'
        options += ' sampling_freq=%d' % sampling_freq
        options += ' word_size=%d' % word_size
        options += ' nb_slots=%d' % nb_slots
        options += ' is_pdm=%d' % is_pdm
        options += ' is_full_duplex=%d' % is_full_duplex
        options += ' is_ext_clk=%d' % is_ext_clk
        options += ' is_ext_ws=%d' % is_ext_ws
        options += ' is_sai0_clk=%d' % is_sai0_clk
        options += ' is_sai0_ws=%d' % is_sai0_ws
        options += ' clk_polarity=%d' % clk_polarity
        options += ' ws_polarity=%d' % ws_polarity
        options += ' ws_delay=%d' % ws_delay
        cmd = 'component %s i2s setup %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def close(self):
        """Close SAI.

        :raises: RuntimeError, if there is any error while closing.
        """
        options = ''
        options += ' itf=%d' % self.id
        options += ' enabled=0'
        cmd = 'component %s i2s setup %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def clk_start(self):
        """Start clock.

        This can be used when the clock is generated by the testbench to start the generation.

        :raises: RuntimeError, if there is any error while starting the clock.
        """
        cmd = 'component %s i2s clk_start %d' % (self.testbench, self.id)
        self.proxy._send_cmd(cmd)

    def clk_stop(self):
        """Stop clock.

        This can be used when the clock is generated by the testbench to stop the generation.

        :raises: RuntimeError, if there is any error while stopping the clock.
        """
        cmd = 'component %s i2s clk_stop %d' % (self.testbench, self.id)
        self.proxy._send_cmd(cmd)

    def slot_open(self, slot: int = 0, is_rx: bool = True, word_size: int = 16, is_msb: bool = True,
            sign_extend: bool = False, left_align: bool = False):
        """Open and configure a slot.

        :param slot: int, optional,
            Slot identifier
        :param is_rx: bool, optional,
            True if gap receives the samples.
        :param word_size: int, optional,
            Slot width in number of bits.
        :param is_msb: bool, optional,
            True if the samples are received or sent with MSB first.
        :param sign_extend: bool, optional,
            True if the samples are sign-extended.
        :param left_align: bool, optional,
            True if the samples are left aligned.

        :raises: RuntimeError, if there is any invalid parameter.
        """
        options = ''
        options += ' itf=%d' % self.id
        options += ' slot=%d' % slot
        options += ' is_rx=%d' % is_rx
        options += ' enabled=1'
        options += ' word_size=%d' % word_size
        options += ' format=%d' % (is_msb | (left_align << 1) | (sign_extend << 1))
        cmd = 'component %s i2s slot_setup %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def slot_close(self, slot: int = 0):
        """Close a slot.

        :param slot: int, optional,
            Slot identifier

        :raises: RuntimeError, if there is any invalid parameter.
        """
        options = ''
        options += ' itf=%d' % self.id
        options += ' slot=%d' % slot
        options += ' enabled=0'
        cmd = 'component %s i2s slot_setup %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def slot_rx_file_reader(self, slot: int = None, slots: list = [], filetype: str = "wav",
            filepath: str = None, encoding: str = "asis", width: int = 0,
            interpolation_ratio_shift : int = 3, interpolation_type : str = "linear"):
        """Read a stream of samples from a file.

        This will open a file and stream it to the SAI so that gap receives the samples.
        It can be used either in mono-channel mode with the slot parameter or multi-channel mode with the
        slots parameter. In multi-channel mode, the slots parameters give the list of slots associated to each channel.
        To allow empty channels, a slot of -1 can be given.

        :param slot: int, optional,
            Slot identifier
        :param slots: list, optional,
            List of slots when using multi-channel mode. slot must be None if this one is not empty.
        :param filetype: string, optional,
            Describes the type of the file, can be "wav", "raw", "bin" or "au".
        :param width: int, optional,
            width of the samples, in case the file is in binary format
        :param filepath: string, optional,
            Path to the file.
        :param encoding: string, optional,
            Encoding type for binary files, can be: "asis", "plusminus" or "pcm" meaning it is "asis" and a conversion is required
        :param interpolation_ratio_shift: int, optional,
            Interpolation ratio shift (only for modulation part - PCM to PDM conversion). Values:
            0 -> no intermolation
            1 -> interpolation by 2 (only linear interpolation)
            2-> interpolation by 4
            3-(DEFAULT)-> interpolation by 8
            4-> interpolation by 16 (only linear interpolation)
            5-> interpolation by 32 (only linear interpolation) ...
        :param interpolation_type: str, optional,
            Interpolation type (only for modulation part - PCM to PDM conversion): linear[default] or IIR

        :raises: RuntimeError, if there is any invalid parameter.
        """
        options = ''
        options += ' itf=%d' % self.id
        if slot is not None:
            options += ' slot=%d' % slot
        for slot in slots:
            options += ' slot=%d' % slot
        options += ' filetype=%s' % filetype
        options += ' filepath=%s' % filepath
        options += ' encoding=%s' % encoding
        options += ' width=%d' % width
        options += ' interpolation_ratio_shift=%d' % interpolation_ratio_shift
        options += ' interpolation_type=%s' % interpolation_type
        cmd = 'component %s i2s slot_rx_file_reader %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def slot_tx_file_dumper(self, slot: int = None, slots: list = [], filetype: str = "wav",
            filepath: str = None, encoding: str = "asis", width: int = 0,
            cic_n: int = 8, cic_m: int = 2, cic_r: int = 8, cic_shift: int = 7, filter_coef: int = 0, wav_sampling_freq : int = -1):
        """Write a stream of samples to a file.

        This will open a file and write to it all the samples received from gap.
        It can be used either in mono-channel mode with the slot parameter or multi-channel mode with the
        slots parameter. In multi-channel mode, the slots parameters give the list of slots associated to each channel.
        To allow empty channels, a slot of -1 can be given. A slot can be given several times in order to push the samples
        to several channels.

        :param slot: int, optional,
            Slot identifier
        :param slots: list, optional,
            List of slots when using multi-channel mode. slot must be None if this one is not empty.
        :param filetype: string, optional,
            Describes the type of the file, can be "wav", "raw", "bin", "au" or "pcm" meaning a conversion is required
        :param encoding: string, optional,
            Encoding type for binary files, can be: "asis", "plusminus"
        :param width: int, optional,
            width of the samples, in case the file is in binary format
        :param filepath: string, optional,
            Path to the file.
        :param cic_n: int, optional,
            Order of the cic (only for modulation part - PCM to PDM conversion): [default = 8] integer\n\
        :param cic_m: int, optional,
            Depth of the delta section of cic (only for modulation part - PCM to PDM conversion): [default = 2]
        :param cic_r: int, optional,
            Ratio of the cic (only for modulation part - PCM to PDM conversion): [default = 4]
        :param cic_shift: int, optional,
            Denormalisation after cic & before filter
        :param filter_coef: int, optional
            Filter coefficient (only for modulation part - PCM to PDM conversion). Values:
            0 [default] no use of post-filter
            1: use lattice_ladder 1
            2:use lattice_ladder 2

        :raises: RuntimeError, if there is any invalid parameter.
        """
        options = ''
        options += ' itf=%d' % self.id
        if slot is not None:
            options += ' slot=%d' % slot
        for slot in slots:
            options += ' slot=%d' % slot
        options += ' filetype=%s' % filetype
        options += ' filepath=%s' % filepath
        options += ' encoding=%s' % encoding
        options += ' width=%d' % width
        options += ' cic_n=%d' % cic_n
        options += ' cic_m=%d' % cic_m
        options += ' cic_r=%d' % cic_r
        options += ' cic_shift=%d' % cic_shift
        options += ' filter_coef=%d' % filter_coef
        options += ' wav_sampling_freq=%d' % wav_sampling_freq
        cmd = 'component %s i2s slot_tx_file_dumper %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def slot_stop(self, slot: int = 0, stop_rx: bool = True, stop_tx: bool = True):
        """Stop a slot.

        This will stop the streamings (file reader or dumper) configured on the specified slot.

        :param slot: int, optional,
            Slot identifier
        :param stop_rx: bool, optional,
            Stop the stream sent to gap.
        :param stop_tx: bool, optional,
            Stop the stream received from gap.

        :raises: RuntimeError, if there is any invalid parameter.
        """
        options = ''
        options += ' itf=%d' % self.id
        options += ' slot=%d' % slot
        options += ' stop_rx=%d' % stop_rx
        options += ' stop_tx=%d' % stop_tx
        cmd = 'component %s i2s slot_stop %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)


class Testbench_spi(object):
    """Class instantiated for each manipulated SPI interface.

    It can used to interact with the SPI.

    :param proxy: Proxy, The proxy object. This class will use it to send command to GVSOC through the proxy connection.
    :param testbench: int, The testbench object.
    :param id: int, optional, The identifier of the SAI interface.
    """

    def __init__(self, proxy: Proxy, testbench: Testbench, itf=0, cs: int = 0):
        self.id = itf
        self.cs = cs
        self.proxy = proxy
        self.testbench = testbench
        self.rx_enabled = False

        self.callback = None
        self.lock = threading.Lock()
        self.condition = threading.Condition(self.lock)
        self.pending_rx_bytes = bytearray()
        self.req = None

    def open(self, is_master: int = 0, polarity: int = 0, phase: int = 0,
            mem_size_log2: int = 16, dummy_cycles: int = 0, frequency : int = 10000000):
        """Open and configure SAI.

        :param word_size: int, optional, Specify the frame word size in bits.
        :param sampling_freq: int, optional, Specify th
        :param nb_slots: int, optional,
            Number of slots in the frame.
        :param is_pdm: bool, optional

        :raises: RuntimeError, if there is any invalid parameter.
        """

        options = ''
        options += ' itf=%d' % self.id
        options += ' enabled=1'
        options += ' cs=%d' % self.cs
        self.is_master = is_master
        options += ' is_master=%d' % (is_master)
        options += ' polarity=%d' % polarity
        options += ' phase=%d' % phase
        options += ' mem_size_log2=%d' % mem_size_log2
        options += ' dummy_cycles=%d' % dummy_cycles
        self.frequency = frequency
        cmd = 'component %s spi setup %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def close(self):
        """Close the spi interface.

        :raises: RuntimeError, if there is any error while closing.
        """
        options = ''
        options += ' itf=%d' % self.id
        options += ' cs=%d' % self.cs
        options += ' enabled=0'
        cmd = 'component %s spi setup %s' % (self.testbench, options)
        self.proxy._send_cmd(cmd)

    def send(self, values: bytes):
        """Send data to the spi.

        This enqueues an array of bytes to be transmitted. If previous transfers are not finished,
        these bytes will be transfered after.

        :param values: bytes, The sequence of bytes to be sent, in little endian byte ordering.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        if (self.is_master == 1):
            # itf_id itf_cs_id number_of_byte_intransfer frequency is_full_duplex is_rx
            cmd = 'component %s spi master_cmd %d %d %d %d 0 0' % (self.testbench, self.id, self.cs, len(values), self.frequency)
        else:
            cmd = 'component %s spi tx %d %d %d' % (self.testbench, self.id, self.cs, len(values))

        # Since we need to send a command and right after the data,
        # we have to keep the command queue locked to avoid mixing our data
        # with another command
        req = self.proxy._send_cmd(cmd, keep_lock=True, wait_reply=False)
        self.proxy.socket.send(values)
        self.proxy._unlock_cmd()

        self.proxy.reader.wait_reply(req)

    def receive(self, size):
        """Launch a receive command on SPI.

        Enqueue the comand to ask a reception on spi. As soon as all the previous
        commands are finished this one is executed.

        :param byte_to_send: bytearray, the bytes that must be sent to spi.
        :param size : the number of byte we want to receie. Must be equal to the length of byte_to_send

        :raises: RuntimeError, If the access generates an error in the architecture.
        """
        self.rx_enable()
        if (self.is_master == 1):
            # itf_id itf_cs_id number_of_byte_intransfer frequency is_full_duplex is_rx
            cmd = 'component %s spi master_cmd %d %d %d %d 0 1' % (self.testbench, self.id, self.cs, size, self.frequency)
        else:
            cmd = 'component %s spi rx %d %d %d' % (self.testbench, self.id, self.cs, size)
        self.proxy._send_cmd(cmd)

    def full_duplex(self, byte_to_send, size):
        """Launch a full duplex transfer on SPI.

        Enqueue the comand to ask a full duplex transfer on spi. As soon as all the previous
        commands are finished this one is executed.

        :param byte_to_send: bytearray, the bytes that must be sent to spi.
        :param size : the number of byte we want to receie. Must be equal to the length of byte_to_send

        :raises: RuntimeError, If the access generates an error in the architecture.
        """
        if(len(byte_to_send) != size):
            return -1
        self.rx_enable()
        if (self.is_master == 1):
            # itf_id itf_cs_id number_of_byte_intransfer frequency is_full_duplex is_rx
            cmd = 'component %s spi master_cmd %d %d %d %d 1 0' % (self.testbench, self.id, self.cs, len(byte_to_send), self.frequency)
        else:
            cmd = 'component %s spi full_duplex %d %d %d' % (self.testbench, self.id, self.cs, len(byte_to_send))
        req = self.proxy._send_cmd(cmd, keep_lock=True, wait_reply=False)
        self.proxy.socket.send(byte_to_send)
        self.proxy._unlock_cmd()

        self.proxy.reader.wait_reply(req)

    def get_received_bytes(self, size=None):
        """Read data from the spi.

        Once reception on the spi is enabled, the received bytes are pushed to a fifo. This method
        can be called to pop received bytes from the FIFO.
        Function blocking until the required number of bytes has been received.

        :param size: int, The number of bytes to be read. If it is None, it returns the bytes which has already been received.

        :return: bytes, The sequence of bytes received, in little endian byte ordering.

        :raises: RuntimeError, If the access generates an error in the architecture.
        """
        self.lock.acquire()

        if size is not None:
            while len(self.pending_rx_bytes) < size and self.rx_enabled:
                self.condition.wait()

            if not self.rx_enabled:
                self.lock.release()
                return None
        else:
            size = len(self.pending_rx_bytes)

        reply = self.pending_rx_bytes[0:size]
        self.pending_rx_bytes = self.pending_rx_bytes[size:]

        self.lock.release()

        return reply


    def rx_enable(self):
        """Enable receiving bytes from the spi.

        Any byte received from the spi either triggers the callback execution if it has been registered,
        or is pushed to a FIFO which can read.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        if(self.rx_enabled == False):
            self.req = self.proxy._get_req()
            self.proxy.reader.register_callback(self.req, self.__handle_rx)
            cmd = 'component %s spi rx_enable %d %d 1 %d' % (self.testbench, self.id, self.cs, self.req)
            self.proxy._send_cmd(cmd)

            self.rx_enabled = True


    def rx_disable(self):
        """Disable receiving bytes from the spi.

        :raises: RuntimeError, if the access generates an error in the architecture.
        """
        self.proxy.reader.unregister_callback(self.req)
        cmd = 'component %s spi rx_enable %d %d 0' % (self.testbench, self.id, self.cs)
        self.proxy._send_cmd(cmd)

        self.lock.acquire()
        self.rx_enabled = False
        self.condition.notify_all()
        self.lock.release()


    def __handle_rx(self):
        self.lock.acquire()
        reply = self.proxy.socket.recv(1)
        if self.callback is not None:
            self.callback[0](1, reply, *self.callback[1], **self.callback[2])
        else:
            self.pending_rx_bytes += reply
            self.condition.notify()
        self.lock.release()


    # def flush(self):
    #     req = self.proxy._get_req()
    #     cmd = 'component %s uart flush %d %d' % (self.testbench, self.id, req)
    #     self.proxy._send_cmd(cmd)
    #     self.proxy.reader.wait_reply(req)

    # def clear(self):
    #     req = self.proxy._get_req()
    #     cmd = 'component %s uart clear %d %d' % (self.testbench, self.id, req)
    #     self.proxy._send_cmd(cmd)
    #     self.proxy.reader.wait_reply(req)

    #     self.lock.acquire()
    #     self.pending_rx_bytes = bytearray()
    #     self.lock.release()
