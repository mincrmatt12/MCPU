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

0b10 - INT registers (internal to cpu)

---

0b01 - ROM/Flash/Ram (remappable region 2)

---

0b00 - ROM/Flash/Ram (remappable region 1)

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

## Memory layout

As summarized above, the memory space is divided into 4 groups. The bottom two are remappable using the `MEM_LAYOUT` register:

```
fedcba9876543210
|  MEM_LAYOUT  |
            bbaa
             | \- region 1 target (MEM_LAYOUT_R1T)
             \--- region 2 target (MEM_LAYOUT_R2T)
```

Both values look up from:

| ID | Target |
| --- | :---- |
| `0b00` | Internal ROM |
| `0b01` | Internal SRAM |
| `0b10` | External SDRAM (must be configured first) |

Various peripherals (described in other documents) are mapped in the third/fourth regions. The first half-kilobyte (from 0x80000000-0x80000200) is reserved
for CPU-internal registers, which are described in the next section.

## CPU registers

### Interrupt handling

#### `IRQ_BASE` - `0x8000 0000`

```
fedcba9876543210fedcba9876543210
|         IRQ_BASE             |
iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii
                               \- base address (IRQ_BASE); rw
```

The base address for interrupt handling routines. Interrupts will begin execution at `IRQ_BASE | (irq << 4)`.
This register is shadowed, only writes to the high word will cause it to update. As a result, you should always write
it low-first.

#### `IRQ_EN` - `0x8000 0004`

```
fedcba9876543210
|    IRQ_EN    |
```

Interrupt enable bitmask. Each bit corresponds to an interrupt, of which the MCPU implements 16.

### Task contexts

#### `TASK_ACTIVE` - `0x8000 0010`

```
fedcba9876543210
| TASK_ACTIVE  |
```

Sets/checks the active task context. Writes _immediately_ change the active context, leaving the old context in such a state that it will continue at the next instruction.

#### `TASK_REG_x_y` - `0x8000 0100 + x*0x40 + y*4`

```
fedcba9876543210fedcba9876543210
|         TASK_REG             |
rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr
                               \- task register (TASK_REG); rw
```

Read/set a task register.

### Memory mapping

#### `MEM_LAYOUT` - `0x8000 0040`

```
fedcba9876543210
|  MEM_LAYOUT  |
            bbaa
             | \- region 1 target (MEM_LAYOUT_R1T); rw
             \--- region 2 target (MEM_LAYOUT_R2T); rw
```

Assigns the current memory mapping setup for the two remappable regions.
