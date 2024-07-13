Target Control
--------------

Introduction
............

To simulate entire systems, device models such as flash, cameras, microphones, or speakers can be
connected to chip models to interact with them. Although this is useful for reproducing real system
behaviors, it may not be convenient for dedicated tests where fine control over chip interactions
is preferred.

The target control feature allows a Python script to connect to the GVSOC engine to interact with
the models from the script. This enables:

- Controlling the simulation engine (running, stopping, stepping, etc.).
- Injecting memory-map requests to interact with the simulated system and retrieve memory content
  for debugging.
- Interacting with pads to inject or extract data, such as replicating microphone behavior with
  precise control over the data.

Compared to real device models, the target control feature is useful when dynamic decisions need to
be made, which can be easily implemented in a Python script.

The API is thread-safe, allowing multiple threads to call any method simultaneously. However, this
can make the user script complex and should be avoided when possible. Multiple threads controlling
the engine simultaneously can easily lead to deadlocks if not done properly.

The following sections provide an overview of the target control feature. See the
:ref:`target_control_ref` section for a detailed API description.

Usage
.....

To use this feature, a Python script must be provided to GVSOC through the
*--target-control=<path to script>* option.

The script will be dynamically imported by the GVSOC runner, so its extension must be *.py*. The
path can be either absolute or relative to the working directory where GVSOC is launched.

When GVSOC is launched with this option, the runner will start two processes. One runs the GVSOC
engine, which opens a proxy socket waiting for a connection from the Python script. The other
process executes the Python script, which connects to the listening socket. Both processes then run
in parallel.

This entire procedure is handled automatically by the runner when the *--target-control* option is
specified.

Python Script Format
....................

When the *--target-control* option is given, the GVSOC runner dynamically imports the specified
Python script, looks for a method called *target_control*, and executes it in a separate process
parallel to the GVSOC process.

The *target_control* method must have the following prototype:

.. code-block:: python

    def target_control(gv: gvsoc.gvsoc_control.Proxy) -> int:

The argument is an instance of a GVSOC proxy that the *target_control* script can use to interact
with GVSOC. It provides an API for interacting with the engine and devices.

The script must perform all interactions with GVSOC and then return an integer as a return status,
which makes GVSOC exit with this status. This status is then passed to the shell as the exit code.

Engine Control
..............

The GVSOC proxy Python class (gvsoc.gvsoc_control.Proxy) provides methods for controlling the
simulation progress.

When the *target_control* method of the user script is called, the simulation is stopped at time 0,
and the script must call methods of the provided class to control it.

The most basic way to control the simulation is to call the *run* method:

.. code-block:: python

    gv.run()

This method is asynchronous and returns immediately. Meanwhile, the GVSOC engine runs the simulation
in parallel, as the engine and script run in separate processes.

The simulation continues until a stop event occurs, either triggered by the script or the simulated
software.

The script can stop the engine by calling the *stop* method, which is also asynchronous and returns
immediately.

The simulated software can also stop the engine by calling a method from the *target/gvsoc.h*
header:

.. code-block:: C

    static inline void gv_stop();

This function uses semi-hosting to interact with the GVSOC engine, resulting in a slight runtime
impact.

Since the *run* and *stop* methods are asynchronous, they may return before the command is
processed. Methods are available to block the caller until the command is acknowledged.

After calling *run*, the *wait_running* method can be used to block the caller until the engine is
running.

Similarly, after calling *stop*, the *wait_stopped* method can be used to block the caller until
the engine stops.

This method of controlling the simulation is not very precise, as it is asynchronous. Running the
simulation twice may yield different results, as commands are processed at different times.

To achieve predictable simulations, the step mode can be used by providing a duration in picoseconds
to the *run* method. This runs the simulation for the specified duration and then stops:

.. code-block:: python

    gv.run(1000000)  # Runs for 1us and stops. Caller is blocked until duration elapses.

This method is synchronous. The caller is blocked until the duration is simulated. The simulation
may stop during the specified duration, but the caller will remain blocked until the duration elapses.

Trace Control
.............

GVSOC system traces can be dynamically controlled through the *trace_add*, *trace_remove*, and
*trace_level* methods.

These methods behave similarly to the corresponding command-line options but are processed
dynamically, making them useful for configuring specific time intervals.

Similarly, VCD traces can be dynamically controlled through the *event_add* and *event_remove*
methods.

Memory-Map Accesses
...................

To inject memory-mapped accesses from the Python script into the simulated system, a reference to
a proxy router must first be retrieved. This proxy router is a special model connected to the main
system interconnect to convert access requests from the script into real system accesses.

This reference is retrieved by instantiating the *gv.gvsoc_control.Router* class:

.. code-block:: python

    router = gv.gvsoc_control.Router(gv)

Accesses can then be performed by calling *mem_read* and *mem_write* on this instance:

.. code-block:: python

    array = router.mem_read_int(0x1c000000, 16)
    router.mem_write_int(0x1c000000, 16, array)

Convenience methods are also available for simpler data retrieval or writing, such as using an
integer format.

These methods are blocking and do not require time to progress, so they can be called when the
engine is stopped.

I2S Interface
.............

The target control feature allows interaction with the I2S pads to inject or extract data.

A dedicated device model called *testbench* is connected to the simulated chip's pads and interacts
with the Python script.

To interact with it, instantiate the *gv.gvsoc_control.Testbench* class similarly to the *Router*
class:

.. code-block:: python

    testbench = gv.gvsoc_control.Testbench(gv)

Then, instantiate the *gv.gvsoc_control.Testbench_i2s* class for each I2S interface to be
controlled:

.. code-block:: python

    i2s_0 = testbench.i2s_get(0)

This instance can be used to set up the interface. First, configure the interface itself, such as
setting the sampling rate, clock type, and data width:

.. code-block:: python

    i2s_0.open(sampling_freq=44100, word_size=16, nb_slots=1, is_ext_clk=True, is_ext_ws=True)

Next, configure the slots if the interface uses the TDM feature:

.. code-block:: python

    i2s_0.slot_open(slot=0, is_rx=True, word_size=16, is_msb=True, sign_extend=False, left_align=False)

Finally, configure how to interact with the data pads. The most common method is to use WAV files.

To inject data into the I2S interface, use the following reader:

.. code-block:: python

    i2s_0.slot_rx_file_reader(slot=0, filetype="wav", filepath='sound_in.wav')

The specified WAV file is opened and streamed into the specified TDM slot. One file per slot is
required. The path is relative to the directory where GVSOC is launched or can be absolute.

To extract data, use a file dumper:

.. code-block:: python

    i2s_0.slot_tx_file_dumper(slot=1, filetype="wav", filepath='sound_out.wav')

Once everything is set up, start the clock:

.. code-block:: python

    i2s_0.clk_start()

At this point, commands have been enqueued to configure the desired interactions on the pad. The
simulation can now run for a while to let the testbench inject and extract data from the interface.

Once finished, everything can be closed:

.. code-block:: python

    i2s_0.clk_stop()
    i2s_0.slot_close(slot=0)
    i2s_0.close()