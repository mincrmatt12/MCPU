--
geometry: margin=1in
---

_required reading:_

- [isa reference](./isa.md)

# MCPU System Model

As stated in the ISA documentation, the MCPU is a fundamentally 32-bit CPU, with 14 general purpose registers (+ one zero-register and program counter). The CPU accesses 
the rest of the system (and indeed, most of its own configuration) through a variety of memory accesses.

To the CPU, the address space is 32-bit byte-addressed. In reality, it is 31-bit word-addressed, with some (but not all) bus targets offering data mask lines to emulate
byte-access. Due to how the address-modes in load/store instructions works, the address space is broken into 4 quadrants, based on the upper 2 bits of the byte-address.

```
----------

0b11 - VRAM/GPU registers (on fundamentally separate bus)

---

0b10 - INT registers (internal to cpu/peripherals)

---

0b01 - SDRAM

---

0b00 - ROM/Flash/Ram (remappable region)

----------
```

## Execution & Exceptions

The MCPU starts execution at address 0. To be precise, it starts execution by handling a virtual interrupt with code 0, whose handler is by default at address 0.
Execution continues according to the ISA, with only minor external modifications. The largest of these is the interrupt engine.

Interrupts/exceptions are handled by doing the following three things:
- switching to task context 3
- setting r14 to the previous value of `TASK_ACTIVE`
- jumping to `(IRQ_BASE | (irq << 4))`

This mentions the second major "extension" to the pure ISA: task contexts. A task context is a full set of registers, and the CPU is always executing from one.
There are 4, numbered 0-3; the CPU starts in context 0. While they are all technically available for general purpose computation, it is highly recommended
to leave context 3 exclusively for interrupts, as every interrupt will otherwise irretrievably clobber the program counter and r14. 

Switching between contexts is achieved by writing to an internal cpu register (`TASK_ACTIVE`). The new task begins execution as if it had just finished
executing an instruction, i.e. at the _next_ instruction relative to its current program counter. Bootstrapping contexts is achieved using the memory-mapped
versions of the CPU registers, available at `TASK_REG_<task>_<n>`, i.e. for r6 in context 1 use `TASK_REG_1_6`. 

An example interrupt handler is shown:

```asm

.global TimerValue
.org IRQ_BASE + TIMER_IRQ << 4
TimerInterrupt:
// increment a global variable
ld r0, [TimerValue]  // 4 bytes
add r0, r0, 1        // 2 bytes
st r0, [TimerValue]  // 4 bytes
st r14, [TASK_ACTIVE] // 4 bytes

```

There are a few things of note here:

- the handler is _not_ in separate routine, since it fits in 16 bytes.
- the handler returns by setting TASK_ACTIVE to whatever r14 was at the entry to the handler

This final instruction for returning from an interrupt is common enough that it may be prudent to define a macro to save
typing it out all the time.


