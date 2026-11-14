# Mailbox Model

A device model for mailbox-based inter-core or component communication. 
Each mailbox slot stores two 32-bit words `LETTER` and exposes send/receive control registers and interrupt outputs.

## Configuration

Create the mailbox with a slot count:

```python
mailbox = devices.mailbox.mailbox.Mailbox(self, "mailbox", size=10)
```

The `size` property determines how many slots the model creates and how many pairs of send/receive IRQ ports it exposes.

## Memory map

Each mailbox slot occupies a `0x100` byte address window. For a mailbox mapped at base address `BASE`, slot `i` is addressed at:

```
BASE + (i * 0x100) + register_offset
```

| Register | Offset | Access in current implementation |
|---|---:|---|
| Send status | `0x00` | Not implemented |
| Send set | `0x04` | Write |
| Send clear | `0x08` | Write |
| Send enable | `0x0C` | Write |
| Receive status | `0x40` | Not implemented |
| Receive set | `0x44` | Write |
| Receive clear | `0x48` | Write |
| Receive enable | `0x4C` | Write |
| Letter 0 | `0x80` | Read/write |
| Letter 1 | `0x8C` | Read/write |

Letter accesses read or write a 32-bit value. Control-register reads are not handled by the current C++ request method.

## Interrupt interfaces

The Python configurator provides these connections for each slot:

- `o_SND_IRQ(id, interface)` binds `irq_snd_<id>`.
- `o_RCV_IRQ(id, interface)` binds `irq_rcv_<id>`.

For example, map the device and connect slot 0’s receive IRQ to a PLIC input:

```python
mailbox = devices.mailbox.mailbox.Mailbox(self, "mailbox", size=10)

ico.o_MAP( mailbox.i_INPUT(), "mailbox", base=0x20000000, size=0x1000, rm_base=True)

mailbox.o_RCV_IRQ(0, plic.i_IRQ(0))
```

Connect the output to the interface that should receive the interrupt. The slot index passed to `o_SND_IRQ` or `o_RCV_IRQ` must be less than `size`.

## Current control behavior

In the current C++ implementation:

- Writing `1` to `SND_SET` sets an internal send-set bit. If the receive-enable bit is set, it asserts that slot’s `irq_rcv` output.
- Writing `1` to `RCV_SET` sets an internal receive-set bit. If the send-enable bit is set, it asserts that slot’s `irq_snd` output.
- `SND_EN` and `RCV_EN` writes set their respective enable bits.
- The clear-register cases clear the corresponding `clear` bit and the associated set bit.

## Build and test

The Python configurator adds `devices.mailbox.mailbox.cpp` as a source and selects the component model `devices.mailbox.mailbox`.
