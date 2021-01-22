---
geometry: margin=1in
---

# MCPU ISA

The MCPU uses a RISC-style instruction set, using 16/32-bit long instructions.

## Registers

The MCPU has 16 registers. Of these, register 15 is the PC and register 0 is a constant zero (mips-style).

## Assembler syntax and instruction encoding

### General encoding

Instructions follow one of the following formats. The formats are referred to by the `[LETTER]` in this document, but
can also be referred to either as "short" or by the length of the immediate.

```
      high           low
|--------------|---------------|
|rs||ro|0| opc ||rs||ro|0| opc |  short mode             "S" 
|rs||ti|0| opc ||rs||ti|0| opc |  short mode with timm   [A]
|rd||   imm    ||rs||ro|1| opc |  long mode              [L]
|rd||       bigimm     |1| opc |  long mode with bigimm  [B]
|rd||     mediimm  ||ro|1| opc |  long mode with mediimm [M]
|rd||   msmimm   |FF|ro|1| opc |  long mode with msmimm  [F]
|rd|| smimm  |FF|rs||ro|1| opc |  long mode with smimm   [T]
```

The fake encoding `"S"` refers to the hybrid long-short encoding where one halfword is used and it is duplicated in the CPU into
the high and low bytes of the processed instruction. This same process occurs for `[A]` encoding, however it is meaningfully
different from the `[L]` encoding and so warrants its own designation.

### Memory access

Memory reads on the MCPU load parts of a 16-byte halfword in various ways into a single register.

Specifically, there are four _load operations_:

- replace high halfword
- replace low halfword
- replace register zero extending
- replace register sign extending

Additionally, the _size_ of the load operation can be defined as either a byte or the entire halfword.
Loaded bytes are zero-extended unless explicitly using the sign-extending load version.

This does present a slight problem in terms of addressing, as the MCPU "soc" thing is designed around a 16-byte
large word. To solve this, the MCPU still uses byte addressing. When accessing words, the low bit is ignored and when accessing
bytes it is used to select which byte to read/write. On bus targets which don't support byte-writes, the behaviour is undefined.

There are three addressing modes. The only one accessible in short encoding (and which isn't accessible in long encoding) is
also the simplest, loading/writing from/to `[ro]`.

The two other addressing modes are selected by the _address mode_ bit in the opcode.

They are either:

   - `[smimm + rO + rS << FF]` (the "generic" mode)
    
     or

   - `[msmimm + FF << 30 + rO]` (the "simple" mode)

In general, the "simple" mode is more suited for MMIO and internal RAM. This is because the SDRAM occupies all of the address space
that a constant immediate can access. To combat this, the "simple" mode uses two bits to instead specify which "section" of the 
address space you want.

The "generic" mode is designed for normal pointer / register relative loading.

Stores are handled with very similar encoding. The only allowed _store operations_ are for high/low halfword, which specifies which part of the register
is to be written.

The assembler syntax for load/store uses two main instructions, `ld` and `st`, with various suffixes for the different modes. This gives the general form

```
ld[.(b)][.(slh)]
```

and

```
st[.b].(lh)
```

where `.b` corresponds to loading/storing a byte, and where `l` is replacing/storing from the low halfword, `h` is for the high halfword, and `s` is for sign-extension. The default 
for loading is zero-extension.

The syntax for specifying addresses is fairly lenient, as the assembler verifies that the requested specification is actually fulfillable by the encoding after parsing. The specification looks like the following
psuedo BNF:

```
specification: '[' component ( '+' component )+ ']'

component: immediate / register

immediate: NUMBER / HEXNUMBER / BINARYNUMBER

register: regname ( '<<' NUMBER )

regname: /r\d\d?/
```

#### Example assembly

```
ld r1, [0xC00000148] 
  ; gives an encoding using the simple mode, 
  ;    with FF inferred as 0b11, 
  ;    imm inferred as 0x148, 
  ;    and rO defaulted to r0 (the always zero register)

ld.h r2, [r3 + 16 + r1 << 1]  
  ; gives an encoding using the generic mode, 
  ;    with FF set to 1, 
  ;    rO set to r3, 
  ;    rS set to r1 and 
  ;    imm set as 16.

ld.b.l r4, [r3] 
  ; gives a short encoding, with rO set to 3

st.b.h r7, [r5 + -0xa] 
  ; gives an encoding using the generic mode, 
  ;    with FF defaulted to 0, 
  ;    rO set to r5, 
  ;    imm set to -0xa, and 
  ;    rS defaulted to r0.
```

#### Specific encoding

The opcode for load/store instructions looks like `00KSTTM`, where the letters correspond to entries in the following tables:

| `K` | Meaning |
| - | ------- |
| 0 | Load |
| 1 | Store |

| `S` | Meaning |
| - | ------- |
| 0 | Byte |
| 1 | Halfword |

| `TT` | Meaning |
| -- | ------- |
| 00 | Zero-extended [^1] |
| 01 | Sign-extended [^1] |
| 10 | Replace low halfword |
| 11 | Replace high halfword |

[^1]: invalid in combination with K=1 store

| `M` | Meaning^[ignored in favor of the `[rO]` mode for short instructions] |
| - | -------- |
| 0 | Generic address mode |
| 1 | Simple address mode |

---

- Load/store instructions with `M = 0` use the `[T]`-type encoding and with `M = 1` the `[F]`-type encoding.
- Load/store instructions support short encoding.

### ALU instructions

The MCPU ALU supports a variety of operations with a variety of potential operands.

The _alu-operations_ supported are:

- add / subtract
- shift left / right
- logical shift left / right
- or / nor
- eor / enor
- and / nand

These operations can be supplied with one of four different _alu-operand-styles_:

- `rs, ro`
- `ro, mediimm`
- `rs, ro << FF+1`
- `rs, ro >> FF+1`

Additionally, for better short-mode encoding, the second operation style is replaced with `rs, timm` if `L = 0`.

The assembler syntax for these follows the following style:

`<op> rd, op1, op2`

where `op1` can be any register and `op2` can be either:

- an immediate
- a register
- a register shifted by up to 4 in either direction

`op` can be one of

- `add`: addition
- `sub`: subtraction
- `[l]s(lr)`: (logical) shift left/right
- `[e][n]or`: (exclusive) (inverted) or
- `[n]and`: (inverted) and

#### Example assembly

```
add r1, r2, 3 
   ; gives a long encoding with SS = 01 
   ;     where mediim is 3 and 
   ;     ro is 3

enor r1, r1, r5 
   ; gives a short encoding with SS = 00
   ;     where rs = rd = r1
   ;     where ro is r5

or r4, r2, r3 << 3
   ; gives a long encoding with SS = 10
   ;     where rd is r4,
   ;     where rs is r2,
   ;     where ro is r3 and
   ;     where FF is 2

sub r2, r2, 1 
   ; gives a short encoding with SS = 10
   ;    where rd = rs = r2 and
   ;    where timm = 1
```

#### Specific encoding

The opcode for ALU instructions looks like `1OOOOSS` where the letters correspond to the following tables:

| `OOOO` | Meaning |
| ---- | ------- |
| 0000 | add |
| 0001 | sub |
| 0010 | sl |
| 0011 | sr |
| 0100 | lsl |
| 0101 | lsr |
| 1000 | or |
| 1001 | eor |
| 1010 | and |
| 1100 | nor |
| 1101 | enor |
| 1110 | nano |

| `SS` | Meaning |
| --- | ------ |
| 00 | Operands are `rs` and `ro` |
| 01 | Operands are `ro` and `mediimm`^[When `L = 0`, this is replaced by `rs` and `timm`] |
| 10 | Operands are `rs` and `ro << FF` |
| 11 | Operands are `rs` and `ro >> FF` |

---

- ALU instructions use the following encodings:
  - With `SS = 00`, encoding `[L]` is used. 
    - Short mode is supported
  - With `SS = 01`, encoding `[M]` is used.
    - In short mode, encoding `[A]` is used instead.
  - With `SS = 10` or `SS = 01`, encoding `[T]` is used.

### Moves and Jumps

MCPU lumps these together as they both support conditions. In this way, we can both have (fairly) cheap jumps
and condition movs (which are useful for doing carry-addition since there aren't any flags.) For simplicity, these instructions
are referred to simply as MOVs.

In general, there are four classes of operation that a mov can do:

- Load an immediate
- Jump to the value in a register (note this is the only instruction that can read 3 registers at once, and takes a bit longer)
- Write either of its operands (optionally with a constant added) to a register

It can do these operations provided that a selectable condition is met:

- equal/notequal
- signed/unsigned less than
- signed/unsigned greater or equal
- always
- bit set (op1 & op2 is nonzero)

All operations other than load immediate use all the possible chunks of an MCPU instruction.

By default the two operands that are compared are registers, however nonzero FF bits can change this.

| FF | Meaning |
| -- | ------ |
| 01 | op1 is instead smimm |
| 10 | op2 is instead smimm |
| 11 | smimm is added to the value that is written / jumped to |

In `L = 0` mode, the FF bits are always masked. This means that pointer jumps can be written using
short-encoding, as well as simple movs.

Note that the value loaded into the target when using `FF = 01/10` is still the original register.

The assembly syntax follows one of two general formats:

```
jmp[.CC]

mov[.CC]
```

where CC is one of the following condition codes (or omitted for always):

| Code | Meaning |
| ---- | -------- |
| `eq` | Operands compare equal |
| `ne` | Operands do not compare equal |
| `lt` | Operand 1 is less than operand 2 |
| `slt` | Operand 1 is less than operand 2, signed |
| `ge` | Operand 1 is greater or equal to operand 2 |
| `sge` | Operand 1 is greater or equal to operand 2, signed |
| `bs` | Operand 1 & operand 2 == operand 2 |

The arguments to these instructions are written as

```
rd, TARGET[, op1, op2]
```

or in the case of `jmp`, rd is ommited and interpreted as `pc`. Similarly to how addresses are interpreted,
the validity checking of arguments is handled not by syntax but by further verification. Therefore, `TARGET`
can be either an immediate, a register, or a register plus an immediate. Similarly, op1 and op2 can either be registers
or immediates.

The operands can be omitted for a unconditional `mov` / `jmp`.

There are also a few psuedo-conditions (which are implemented by swapping operands):

| Code | Meaning & Implementation |
| ----- | --------------- |
| `gt` / `sgt` | Operand 1 is greater than operand 2, implemented as `op2 < op1` |
| `le` / `sle` | Operand 1 is less than or equal to operand 2, implemented as `op2 >= op1` |

Due to the encoding, it should be obvious that at most one operand can be an immediate, and if one does that
it becomes impossible to add that value to to a register. Similarly, if using a `mov`, the target must either be
an immediate or one of the operands (possibly with a constant addded.)

Internally, the assembler treats both syntaxes as the same psuedoinstruction, and then decides whether or not to
use an encoding with a `JUMP` operation or to use a `MOV` with the destination set as the program counter. The
encoding with a `JUMP` operation is favoured when possible.

#### Assembly examples

```
mov r1, 0x5554 
   ; uses [B] encoding with operation == LI
   ;     rd = r1
   ;     imm = 0x5554
   ;     CC = always

jmp 0x5554
   ; written as [B] encoding with
   ;     rd = pc
   ;     imm = 0x5554
   ;     CC = always
   ; although it could use jmp r0 + imm, this is unnecessarily complicated.

jmp pc ; inf loop
   ; written as [T] encoding with operation = JUMP
   ;     rd = pc
   ;     CC = always

jmp pc + 0x148 ; relative jump
   ; written as [T] encoding with operation = JUMP
   ;     rd = pc
   ;     smimm = 0x148
   ;     CC = always
   ;     FF = 11

mov.lt r1, 0x5554, r2, r3
   ; written as [T] encoding with operation = LI
   ;     rd = r1
   ;     imm = 0x5554
   ;     rs = r2
   ;     ro = r3
   ;     FF = 00
   ;     CC = lt

mov.bs r2, r3, r4, r3
   ; written as [T] encoding with operation = LOP2
   ;     rd = r2
   ;     rs = r4
   ;     ro = r3
   ;     CC = bs

mov.bs r3, r2 + 0x134, r4, r2
   ; written as [T] encoding with operation = LOP2
   ;    rd = r3
   ;    ro = r4
   ;    rs = r2
   ;    smimm = 0x134
   ;    FF = 11
   ;    CC = bs

mov r5, r6
   ; written as "S" ([T]) encoding with operation = LOP2
   ;    rd = rs = r5
   ;    ro = r6
   ;    CC = al
   ;    FF is masked when L = 0

jmp.le r4 + 0x12, r6, r7
   ; written as [T] encoding with operation = JUMP
   ;    rd = r4
   ;    smimm = 0x12
   ;    rs = r7
   ;    ro = r6
   ;    CC = ge
   ;    FF = 11

jmp.ne pc + -0x145, r5, r0
   ; writen as [T] encoding with operation = JUMP
   ;    rd = pc
   ;    smimm = -0x145
   ;    rs = r5
   ;    ro = r0
   ;    CC = ne
   ;    FF = 11

jmp.gt r3, r5, 0x55
   ; written as [T] encoding with operation = JUMP
   ;   rd = r3
   ;   ro = r5
   ;   smimm = 0x55
   ;   CC = lt
   ;   FF = 10
```

#### Specific encoding

The opcode for movs looks like `01CCCOO` where the letters correspond to entries in these tables:

| `CCC` | Meaning |
| --- | -------- |
| 000 | Less than |
| 001 | Signed less than |
| 010 | Greater or equal |
| 011 | Signed greater or equal |
| 100 | Equal |
| 101 | Not equal |
| 110 | Bits set |
| 111 | Always |

| `OO` | Meaning |
| -- | ------- |
| 00 | Load immediate |
| 01 | Jump |
| 10 | Load operand 1 (rs) |
| 11 | Load operand 2 (ro) |

---

- Most mov operations use `[T]` encoding. The only exceptions are:
  - When `OO = 00 and CCC = 111`, use `[B]` encoding.
  - When `OO = 00 and CCC != 111`, use `[L]` encoding.
  - When `OO = 00 and CCC = 111 and L = 0`, use `[A]` encoding.
- When encoding for instructions with `[T]` encoding, use `FF` from the following table:

| `FF` | Meaning |
| -- | --------- |
| 00 | No special operations |
| 01 | Replace operand 1 with the value in `smimm` |
| 10 | Replace operand 2 with the value in `smimm` |
| 11 | Take the "result value" -- what would be written / jumped to -- and before using it add `smimm` to it |

- All immediates used are sign-extended.
