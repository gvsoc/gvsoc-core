# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Beat-streaming router (``interco/router/router_v2_beat.cpp``) testbench.

Each test instantiates a single ``Router(kind='beat', ...)`` (or two of them
in cascade), drives one or two :class:`StubMaster` schedules through it,
routes them to one or more :class:`StubTarget` s, and inspects the stub
logs. ``build_case`` returns the per-case spec dict; ``Chip`` wires the
components according to ``spec['topology']`` and ``spec['nb_masters']``.

Most tests are smoke tests — they assert only that the simulation exits
cleanly. The few cases that ship with a ``checker`` (see ``testset.cfg``)
parse the master's ``RESP`` log lines and assert specific cycle counts.

Tests
=====

Single-master plumbing
----------------------

single_beat
~~~~~~~~~~~

A single 4-byte read at cycle 10 reaches one target. Smoke test for the
basic decode-and-forward path: master ``SEND``, router ``GRANTED``, fsm
wakes one cycle later, target completes inline, master sees ``RESP``.
Verifies the happy single-master / single-output path.

burst4
~~~~~~

A 4-beat burst (16 bytes total at ``width=4``) from one master to one
target. Verifies that the burst-table slot survives across the
``is_first..is_last`` beats, that the per-burst ``burst_id`` remap is stable
across the round trip, and that ``max_input_pending_size=64`` holds all
in-flight beats without back-pressure.

size_over_width
~~~~~~~~~~~~~~~

A single 8-byte beat sent through a router with ``width=4``. Exercises the
"beat too large" guard in ``req_muxed`` — the router immediately responds
``IO_RESP_INVALID`` and increments the ``errors`` stat without ever forwarding.

Multi-master arbitration
------------------------

two_masters_contend
~~~~~~~~~~~~~~~~~~~

Two masters issue a 4-beat burst at the same cycle to the same target.
Verifies round-robin arbitration *between* bursts (a starting input keeps
the output until ``is_last``) and burst atomicity *within* a burst (no
interleaving of the two streams' beats on the output channel).

Downstream stall behaviour
--------------------------

deny_mid_burst
~~~~~~~~~~~~~~

Target denies the first 2 of a 4-beat burst (``deny_then_done`` with
``deny_count=2`` and ``retry_delay=3``), then accepts. Verifies that the
output's ``stalled`` flag is set on each ``IO_REQ_DENIED``, that the FIFO
holds the head beat across the stall, and that ``retry_muxed`` cleanly
resumes forwarding from the same head.

granted_resp_delay
~~~~~~~~~~~~~~~~~~

Target answers ``IO_REQ_GRANTED`` with a 5-cycle ``resp_delay``. Verifies
the deferred-response path: the router stays out of the way, the master
sees ``GRANTED``, and the target's late ``resp()`` reaches the master via
``resp_muxed``.

deny_then_granted
~~~~~~~~~~~~~~~~~

Target denies the first beat twice, then accepts subsequent attempts with
``IO_REQ_GRANTED`` + ``resp_delay=5``. Exercises the full
``DENY → retry → re-forward → GRANTED → deferred resp`` chain in one test.

fifo_overflow
~~~~~~~~~~~~~

Target denies the first beat once with a long ``retry_delay=10``, so the
output stalls and beats back up in the router FIFO. With
``max_input_pending_size=8`` (2 beats of size 4) and a 4-beat burst, the
3rd and 4th beats are denied *upstream* (the router runs out of FIFO
space). Verifies that ``denied_upstream`` triggers ``retry()`` to the
master once the FIFO drains.

Cascade topologies
------------------

cascade_single
~~~~~~~~~~~~~~

Two routers in series — ``master → router_a → router_b → target`` — with
a single beat. Verifies the chained mapping decode and that each router
adds one cycle of latency (one fsm tick per router).

cascade_burst4
~~~~~~~~~~~~~~

A 4-beat burst through the same cascade. Verifies that beats pipeline
across both routers: the Nth beat reaches the target at
``master_send_cycle + 2``, with a steady-state throughput of 1 beat/cycle.

cascade_deny
~~~~~~~~~~~~

Target denies the first beat of a 2-beat burst once then accepts.
Verifies that the burst-id remap chain survives a deny across two routers
(slot-id remap in router_b is reverted on deny, then re-applied on the
re-forward).

Shared vs split read/write channels
-----------------------------------

These four tests use the test-suite's ``checker`` mechanism to assert
exact RESP cycles, since the whole point of ``shared_rw_channel`` is the
*timing* relationship it produces.

shared_rw_channel_split
~~~~~~~~~~~~~~~~~~~~~~~

Two single-beat bursts to the same output on the same cycle — one read
(``master_a``), one write (``master_b``) — with the default
``shared_rw_channel=False``. Asserts that both ``RESP`` lines land on the
same cycle, proving the read and write channels are independent on the
shared output.

shared_rw_channel_shared
~~~~~~~~~~~~~~~~~~~~~~~~

Same scenario as ``shared_rw_channel_split`` but with
``shared_rw_channel=True``. Asserts that the two ``RESP`` cycles are
exactly one apart, proving R and W serialise through the single shared
slot.

shared_rw_channel_bandwidth_split
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sustained-throughput proof. Two 8-beat bursts (one R, one W) start at
the same cycle on the same output with ``shared_rw_channel=False``.
Asserts that the 16 beats complete in 8 cycles → **2.0 beats/cycle**, and
that beat ``i`` lands on the same cycle for R and W (the channels run
in lockstep).

shared_rw_channel_bandwidth_shared
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Same workload as the split case but with ``shared_rw_channel=True``.
Asserts that the 16 beats complete in 16 cycles → **1.0 beat/cycle** —
exactly half the throughput, since burst atomicity makes the second
burst wait for the first to release the shared channel.
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.router_v2 import Router, RouterConfig, RouterMapping
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


ALL_ADDR = 0xFFFF_FFFF_FFFF_FFFF


def burst(cycle, addr, size, nb_beats=1, burst_id=-1, is_write=False, name=None,
          stride=None):
    d = dict(cycle=cycle, addr=addr, size=size, nb_beats=nb_beats, burst_id=burst_id,
             is_write=is_write)
    if name is not None:
        d['name'] = name
    if stride is not None:
        d['stride'] = stride
    return d


def rule(addr_min=0, addr_max=ALL_ADDR, behavior='done',
         resp_delay=0, retry_delay=0, deny_count=0):
    return dict(addr_min=addr_min, addr_max=addr_max, behavior=behavior,
                resp_delay=resp_delay, retry_delay=retry_delay, deny_count=deny_count)


def build_case(case: str):
    t0_base = 0x1000_0000
    t1_base = 0x1001_0000
    window = 0x1_0000
    ok = [rule('done')]

    def beat_cfg(**kwargs):
        """Shorthand for the common ``RouterConfig(kind='beat', width=4, ...)``."""
        kwargs.setdefault('width', 4)
        return RouterConfig(kind='beat', **kwargs)

    if case == 'single_beat':
        return dict(
            config=beat_cfg(max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base + 0x100, size=4, name='r0')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'burst4':
        return dict(
            config=beat_cfg(max_input_pending_size=64),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                            name='b0')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'two_masters_contend':
        # Two masters issue a 4-beat burst at cycle 10; expect round-robin interleaving
        # between bursts (not within).
        return dict(
            config=beat_cfg(max_input_pending_size=64),
            schedule_a=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                              name='A')],
            schedule_b=[burst(cycle=10, addr=t0_base + 0x40, size=4, nb_beats=4,
                              burst_id=2, name='B')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=2,
        )

    if case == 'size_over_width':
        return dict(
            config=beat_cfg(max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=8, name='oversized')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'deny_mid_burst':
        # Target denies the first 2 matching beats then accepts (simulates a downstream
        # stall on beat 1 and 2 of a 4-beat burst).
        rules_t0 = [rule(behavior='deny_then_done', deny_count=2, retry_delay=3)]
        return dict(
            config=beat_cfg(max_input_pending_size=64),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                            name='b0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    if case == 'granted_resp_delay':
        rules_t0 = [rule(behavior='granted', resp_delay=5)]
        return dict(
            config=beat_cfg(max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=4, name='r0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    if case == 'deny_then_granted':
        # Target denies the first beat twice, then accepts subsequent requests with
        # GRANTED + resp_delay=5. Exercises the DENY → retry → re-forward → GRANTED
        # → deferred resp path end-to-end.
        rules_t0 = [rule(behavior='deny_then_granted', deny_count=2, retry_delay=3,
                         resp_delay=5)]
        return dict(
            config=beat_cfg(max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=4, name='r0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    if case == 'cascade_single':
        # master -> router_a -> router_b -> target. Single beat. Expect 2-cycle
        # router latency (1 per router's arbiter+fsm).
        return dict(
            topology='cascade',
            config=beat_cfg(max_input_pending_size=16),
            config_b=beat_cfg(max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base + 0x100, size=4, name='r0')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'cascade_burst4':
        # master -> router_a -> router_b -> target. 4-beat burst. Expect beats to
        # pipeline: the Nth beat reaches the target at cycle master_send_cycle + 2.
        return dict(
            topology='cascade',
            config=beat_cfg(max_input_pending_size=16),
            config_b=beat_cfg(max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                            name='b0')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'cascade_deny':
        # master -> router_a -> router_b -> target. Target denies first beat once
        # then accepts. Verifies deny propagation and id-remap chain across two routers.
        rules_t0 = [rule(behavior='deny_then_done', deny_count=1, retry_delay=3)]
        return dict(
            topology='cascade',
            config=beat_cfg(max_input_pending_size=16),
            config_b=beat_cfg(max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=2, burst_id=7,
                            name='b0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    if case == 'shared_rw_channel_split' or case == 'shared_rw_channel_shared':
        # Two masters issue a single beat to the same output at the same cycle —
        # one read, one write. With separate R/W channels (default) both forward
        # in the same fsm pass and complete on the same cycle. With a shared
        # channel they serialize and complete one cycle apart.
        shared = case == 'shared_rw_channel_shared'
        return dict(
            config=beat_cfg(max_input_pending_size=16, shared_rw_channel=shared),
            schedule_a=[burst(cycle=10, addr=t0_base + 0x100, size=4, name='R',
                              is_write=False)],
            schedule_b=[burst(cycle=10, addr=t0_base + 0x200, size=4, name='W',
                              is_write=True)],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=2,
        )

    if case == 'shared_rw_channel_bandwidth_split' or case == 'shared_rw_channel_bandwidth_shared':
        # Sustained-throughput proof: two 8-beat bursts (one R, one W) start at
        # the same cycle on the same output. With separate channels both bursts
        # pipeline at 1 beat/cycle/channel and finish together in 8 cycles
        # (16 beats / 2 channels = 8 cycles). With a shared channel they
        # interleave round-robin at 1 beat/cycle total, so the last beat lands
        # 16 cycles after the first one — exactly half the throughput.
        shared = case == 'shared_rw_channel_bandwidth_shared'
        nb_beats = 8
        # max_input_pending_size must hold one full burst per input.
        fifo = nb_beats * 4
        return dict(
            config=beat_cfg(max_input_pending_size=fifo, shared_rw_channel=shared),
            schedule_a=[burst(cycle=10, addr=t0_base, size=4, nb_beats=nb_beats,
                              burst_id=1, name='R', is_write=False)],
            schedule_b=[burst(cycle=10, addr=t0_base + 0x80, size=4,
                              nb_beats=nb_beats, burst_id=2, name='W',
                              is_write=True)],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=2,
        )

    if case == 'fifo_overflow':
        # Force the router's input FIFO to fill: target denies the first beat once
        # with a long retry_delay, so the output stalls and beats back up in the
        # router FIFO. With FIFO = 8B (2 beats) and a 4-beat burst, the 3rd and 4th
        # beats should be DENIED upstream and re-sent after retry().
        rules_t0 = [rule(behavior='deny_then_done', deny_count=1, retry_delay=10)]
        return dict(
            config=beat_cfg(max_input_pending_size=8),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                            name='b0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    raise ValueError(f'Unknown case: {case}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)

        case = TargetParameter(
            self, name='case', value='single_beat',
            description='Which test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        topology = spec.get('topology', 'single')

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        if topology == 'cascade':
            # master -> router_a -> router_b -> target(s)
            router_a = Router(self, 'router_a', config=spec['config'])
            router_b = Router(self, 'router_b', config=spec['config_b'])
            clock.o_CLOCK(router_a.i_CLOCK())
            clock.o_CLOCK(router_b.i_CLOCK())

            master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
            clock.o_CLOCK(master.i_CLOCK())
            master.o_OUTPUT(router_a.i_INPUT(0))

            # router_a forwards through to router_b preserving the address
            # (rm_base=False, remove_offset=0 -> pass-through).
            router_a.o_MAP(router_b.i_INPUT(0), RouterMapping(
                name='through', base=0x1000_0000, size=0x10_0000, remove_base=False))

            for (tname, base, size, rules) in spec['targets']:
                tgt = StubTarget(self, tname, rules=rules, logname=tname)
                clock.o_CLOCK(tgt.i_CLOCK())
                router_b.o_MAP(tgt.i_INPUT(), RouterMapping(
                    name=tname, base=base, size=size))
            return

        router = Router(self, 'router', config=spec['config'])
        clock.o_CLOCK(router.i_CLOCK())

        nb_masters = spec['nb_masters']
        if nb_masters == 1:
            master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
            clock.o_CLOCK(master.i_CLOCK())
            master.o_OUTPUT(router.i_INPUT(0))
        else:
            ma = StubMaster(self, 'master_a', schedule=spec['schedule_a'], logname='master_a')
            mb = StubMaster(self, 'master_b', schedule=spec['schedule_b'], logname='master_b')
            clock.o_CLOCK(ma.i_CLOCK())
            clock.o_CLOCK(mb.i_CLOCK())
            ma.o_OUTPUT(router.i_INPUT(0))
            mb.o_OUTPUT(router.i_INPUT(1))

        for (tname, base, size, rules) in spec['targets']:
            tgt = StubTarget(self, tname, rules=rules, logname=tname)
            clock.o_CLOCK(tgt.i_CLOCK())
            router.o_MAP(tgt.i_INPUT(), RouterMapping(name=tname, base=base, size=size))


class Target(gvsoc.runner.Target):
    gapy_description = 'router_async_v2 testbench'
    model = Chip
    name = 'test'
