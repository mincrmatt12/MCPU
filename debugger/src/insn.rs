// Instruction opcode

use num_derive::FromPrimitive;    
use num_traits::FromPrimitive;

#[derive(FromPrimitive, Debug)]
pub enum AluOp {
    Add  = 0b0000,
    Sub  = 0b0001,
    Sl   = 0b0010,
    Sr   = 0b0011,
    Lsl  = 0b0100,
    Lsr  = 0b0101,
    Or   = 0b1000,
    Eor  = 0b1001,
    And  = 0b1010,
    Nor  = 0b1100,
    Enor = 0b1101,
    Nand = 0b1110
}

#[derive(FromPrimitive, Debug, PartialEq)]
pub enum MovCond {
    Lt   = 0b000,
    Slt  = 0b001,
    Ge   = 0b010,
    Sge  = 0b011,
    Eq   = 0b100,
    Neq  = 0b101,
    Bs   = 0b110,
    Al   = 0b111
}

#[derive(FromPrimitive, Debug, PartialEq)]
pub enum MovOp {
    Mimm = 0b00,
    Jump = 0b01,
    Mrs  = 0b10,
    Mro  = 0b11
}

#[derive(FromPrimitive, Debug)]
pub enum AluSty {
    Reg    = 0b00,
    Imm    = 0b01,
    RegSl  = 0b10,
    RegSr  = 0b11
}

#[derive(FromPrimitive, Debug)]
pub enum LoadStoreDest {
    Zext = 0b00,
    Sext = 0b01,
    LowW = 0b10,
    HighW= 0b11
}

#[derive(FromPrimitive, Debug)]
pub enum LoadStoreMode {
    Generic = 0,
    Simple = 1
}

#[derive(FromPrimitive, Debug)]
pub enum LoadStoreSize {
    Byte = 0,
    Halfword = 1
}

#[derive(FromPrimitive, Debug)]
pub enum LoadStoreKind {
    Load = 0,
    Store = 1
}

#[derive(Debug)]
pub enum InsnOpcode {
    Mov {
        op: MovOp,
        condition: MovCond
    },
    Alu {
        style: AluSty,
        op: AluOp
    },
    LoadStore {
        kind: LoadStoreKind,
        size: LoadStoreSize,
        dest: LoadStoreDest,
        mode: LoadStoreMode
    }
}

#[derive(Debug)]
pub enum InsnContent {
    Short { rd: u8, ro: u8 },
    Tiny  { rd: u8, imm: i8 },
    Long  { rd: u8, imm: i16, rs: u8, ro: u8 },
    Big   { rd: u8, imm: i32 },
    Med   { rd: u8, imm: i32, ro: u8},
    Msm   { rd: u8, imm: i32, ff: u8, ro: u8},
    Sm    { rd: u8, imm: i16, ff: u8, rs: u8, ro: u8}
}

#[derive(Debug)]
pub struct Insn {
    pub content: InsnContent,
    pub opcode: InsnOpcode
}

impl std::fmt::Display for Insn {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { 
        match &self.opcode {
            InsnOpcode::LoadStore {kind, size, dest, mode} =>
            {
                match kind {
                    LoadStoreKind::Load => write!(f, "ld")?,
                    LoadStoreKind::Store => write!(f, "st")?
                };
                match size {
                    LoadStoreSize::Byte => write!(f, ".b")?,
                    _ => {}
                };
                match dest {
                    LoadStoreDest::Sext => write!(f, ".s")?,
                    LoadStoreDest::LowW => write!(f, ".l")?,
                    LoadStoreDest::HighW => write!(f, ".h")?,
                    _ => {}
                };
                // print args
                match (&self.content, mode) {
                    (InsnContent::Short {rd, ro}, _) => write!(f, " r{}, [r{}]", rd, ro),
                    (InsnContent::Sm {rd, imm, ff, rs, ro}, LoadStoreMode::Generic) => 
                        write!(f, " r{}, [{:x} + r{} + r{} << {}]", rd, imm, ro, rs, ff),
                    (InsnContent::Msm {rd, imm, ff, ro}, LoadStoreMode::Simple) =>
                        write!(f, " r{}, [{:x} + r{}]", rd, (imm & !(0b11 << 30)) | ((*ff as i32) << 30), ro),
                    _ => write!(f, "<unk encoding>")
                }
            },
            InsnOpcode::Alu {style, op} => 
            {
                match op {
                    AluOp::Add => write!(f, "add")?,
                    AluOp::Sub => write!(f, "sub")?,
                    AluOp::Sl => write!(f, "sl")?,
                    AluOp::Sr => write!(f, "sr")?,
                    AluOp::Lsl => write!(f, "lsl")?,
                    AluOp::Lsr => write!(f, "lsr")?,
                    AluOp::Or => write!(f, "or")?,
                    AluOp::Eor => write!(f, "eor")?,
                    AluOp::And => write!(f, "and")?,
                    AluOp::Nor => write!(f, "nor")?,
                    AluOp::Enor => write!(f, "enor")?,
                    AluOp::Nand => write!(f, "nand")?
                };
                match (&self.content, style) {
                    (InsnContent::Short {rd, ro}, AluSty::Reg) => write!(f, " r{0}, r{0}, r{1}", rd, ro),
                    (InsnContent::Long  {rd, rs, ro, ..}, AluSty::Reg) => write!(f, " r{}, r{}, r{}", rd, rs, ro),
                    (InsnContent::Tiny  {rd, imm}, AluSty::Imm) => write!(f, " r{0}, r{0}, {1}  // ({1:x})", rd, imm),
                    (InsnContent::Med   {rd, ro, imm}, AluSty::Imm) => write!(f, " r{0}, r{1}, {2}  // ({2:x})", rd, ro, imm),
                    (InsnContent::Sm    {rd, rs, ro, ff, ..}, AluSty::RegSl) => write!(f, " r{0}, r{1}, r{2} << {3}", rd, rs, ro, ff+1),
                    (InsnContent::Sm    {rd, rs, ro, ff, ..}, AluSty::RegSr) => write!(f, " r{0}, r{1}, r{2} >> {3}", rd, rs, ro, ff+1),
                    _ => write!(f, "<unk encoding>")
                }
            }
            InsnOpcode::Mov {condition, op} => 
            {
                match op {
                    MovOp::Jump => write!(f, "jmp")?,
                    _           => write!(f, "mov")?
                };
                match condition {
                    MovCond::Lt   => write!(f, ".lt")?,
                    MovCond::Slt  => write!(f, ".slt")?,
                    MovCond::Ge   => write!(f, ".ge")?,
                    MovCond::Sge  => write!(f, ".sge")?,
                    MovCond::Eq   => write!(f, ".eq")?,
                    MovCond::Neq  => write!(f, ".neq")?,
                    MovCond::Bs   => write!(f, ".bs")?,
                    _ => {}
                };
                match (&self.content, op, condition) {
                    (InsnContent::Big {rd, imm}, MovOp::Mimm, MovCond::Al) => 
                        write!(f, " r{0}, {1}  // ({1:x})", rd, imm)?,
                    (InsnContent::Tiny {rd, imm}, MovOp::Mimm, MovCond::Al) => 
                        write!(f, " r{0}, {1}  // ({1:x})", rd, imm)?,
                    (InsnContent::Long {rd, rs, ro, imm}, MovOp::Mimm, _) => 
                        write!(f, " r{0}, {1}, r{2}, r{3}  // ({1:x})", rd, imm, rs, ro)?,
                    (InsnContent::Sm {rd, rs, ff: 0b11, imm, ..}, MovOp::Mrs, _) =>
                        write!(f, " r{0}, r{1} + {2:x}", rd, rs, imm)?,
                    (InsnContent::Sm {rd, rs, ..}, MovOp::Mrs, _) =>
                        write!(f, " r{0}, r{1}", rd, rs)?,
                    (InsnContent::Sm {rd, ro, ff: 0b11, imm, ..}, MovOp::Mro, _) =>
                        write!(f, " r{0}, r{1} + {2:x}", rd, ro, imm)?,
                    (InsnContent::Sm {rd, ro, ..}, MovOp::Mro, _) =>
                        write!(f, " r{0}, r{1}", rd, ro)?,
                    (InsnContent::Sm {rd, ff: 0b11, imm, ..}, MovOp::Jump, _) =>
                        write!(f, " r{0} + {1:x}", rd, imm)?,
                    (InsnContent::Sm {rd, ..}, MovOp::Jump, _) =>
                        write!(f, " r{0}", rd)?,
                    (InsnContent::Short {rd, ro}, MovOp::Mro, MovCond::Al) =>
                        write!(f, " r{0}, r{1}", rd, ro)?,
                    _ => write!(f, "<unk>")?
                };
                if *op == MovOp::Mimm || *condition == MovCond::Al {
                    write!(f, "")
                }
                else {
                    match &self.content {
                        InsnContent::Long {rs, ro, ..} |
                        InsnContent::Sm {rs, ro, ff: 0, ..} |
                        InsnContent::Sm {rs, ro, ff: 0b11, ..} =>
                            write!(f, ", r{}, r{}", rs, ro),
                        // replace op1
                        InsnContent::Sm {ro, ff: 0b01, imm, ..} =>
                            write!(f, ", {0}, r{1}, // ({0:x})", imm, ro),
                        // replace op2
                        InsnContent::Sm {rs, ff: 0b10, imm, ..} =>
                            write!(f, ", r{0}, {1}, // ({1:x})", rs, imm),
                        _ => write!(f, "<unk>")
                    }
                }
            }
        }
    }
}

// Helper for extracting values (signed version)
fn extract_bits(from: u32, inside: std::ops::Range<usize>) -> i32 {
    assert!(inside.end <= 32);

    let width = inside.end - inside.start;

    assert!(width > 0);

    let raw: i32 = ((from >> inside.start) & ((1 << width) - 1)) as i32;
    // sign-extend and return
    if raw & (1 << width-1) != 0 {
        let mask = (-1i32) & !((1 << width) - 1);
        raw | mask
    }
    else {
        raw
    }
}

fn extract_unsigned_bits(from: u32, inside: std::ops::Range<usize>) -> u32 {
    assert!(inside.end <= 32);

    let width = inside.end - inside.start;

    assert!(width > 0);

    (from >> inside.start) & ((1 << width) - 1)
}

impl Insn {
    pub fn from_word(data: u32) -> Option<Insn> {
        // This u32 has already been duplicate-extended and everything, we just need
        // to work out:
        //  which encoding it uses; and what opcode it is.
        //  first we build the opcode
        let opcode_raw: u32 = extract_unsigned_bits(data, 0..7);
        let is_long = data & (1 << 7) != 0;
        let rd: u8 = extract_unsigned_bits(data, 28..32) as u8;

        let opcode = match (opcode_raw & 0b0_11_00000) >> 5 {
            0b00 => InsnOpcode::LoadStore {
                kind: LoadStoreKind::from_u32(extract_unsigned_bits(opcode_raw, 4..5))?,
                mode: LoadStoreMode::from_u32(extract_unsigned_bits(opcode_raw, 0..1))?,
                dest: LoadStoreDest::from_u32(extract_unsigned_bits(opcode_raw, 1..3))?,
                size: LoadStoreSize::from_u32(extract_unsigned_bits(opcode_raw, 3..4))?
            },
            0b01 => InsnOpcode::Mov {
                op: MovOp::from_u32(extract_unsigned_bits(opcode_raw, 0..2))?,
                condition: MovCond::from_u32(extract_unsigned_bits(opcode_raw, 2..5))?
            },
            _ => InsnOpcode::Alu {
                style: AluSty::from_u32(extract_unsigned_bits(opcode_raw, 0..2))?,
                op: AluOp::from_u32(extract_unsigned_bits(opcode_raw, 2..6))?
            }
        };

        // ABSOLUTELY MASSIVE patterns here for determining the kind
        let content = match (is_long, &opcode) {
            // Exceptions for Tiny mode:
            // ALU-short with SS = 01
            (false, InsnOpcode::Alu{style: AluSty::Imm, ..}) |
            // mov tiny
            (false, InsnOpcode::Mov{op: MovOp::Mimm, condition: MovCond::Al}) =>
                InsnContent::Tiny {
                    rd: rd,
                    imm: extract_bits(data, 8..12) as i8
                },
            // All other short:
            (false, _) =>
                InsnContent::Short {
                    rd: rd,
                    ro: extract_unsigned_bits(data, 8..12) as u8
                },
            // Only mov instructions for set immediate use [B]
            (true, InsnOpcode::Mov{op: MovOp::Mimm, condition: MovCond::Al}) =>
                InsnContent::Big {
                    rd: rd,
                    imm: extract_bits(data, 8..28)
                },
            // ALU with style = imm in long use M
            (true, InsnOpcode::Alu{style: AluSty::Imm, ..}) => 
                InsnContent::Med {
                    rd: rd,
                    imm: extract_bits(data, 12..28),
                    ro: extract_unsigned_bits(data, 8..12) as u8
                },
            // Alu with style-reg use long-mode
            (true, InsnOpcode::Alu{style: AluSty::Reg, ..}) |
            // Mov with op = Mimm and not al
            (true, InsnOpcode::Mov{op: MovOp::Mimm, ..}) =>
                InsnContent::Long {
                    rd: rd,
                    imm: extract_bits(data, 16..28) as i16,
                    rs: extract_unsigned_bits(data, 12..16) as u8,
                    ro: extract_unsigned_bits(data, 8..12) as u8
                },
            // Load-store with M = 1 use F
            (true, InsnOpcode::LoadStore{mode: LoadStoreMode::Simple, ..}) =>
                InsnContent::Msm {
                    rd: rd,
                    imm: extract_bits(data, 14..28),
                    ff: extract_unsigned_bits(data, 12..14) as u8,
                    ro: extract_unsigned_bits(data, 8..12) as u8
                },
            // All other longs use T encoding
            (true, _) => 
                InsnContent::Sm {
                    rd: rd,
                    imm: extract_bits(data, 18..28) as i16,
                    rs: extract_unsigned_bits(data, 12..16) as u8,
                    ro: extract_unsigned_bits(data, 8..12) as u8,
                    ff: extract_unsigned_bits(data, 16..18) as u8
                }
        };
        Some(Insn{
            content: content,
            opcode: opcode
        })
    }

    pub fn dissasemble<'a, T>(mut values: T) -> Option<Vec<Insn>>
        where T: Iterator<Item=&'a u8>
    {
        let mut out_buf = Vec::<Insn>::new();

        loop {
            let mut data = 0u32;
            match values.next() {
                None => break,
                Some(x) => data |= *x as u32
            }
            data |= (*values.next()? as u32) << 8;
            data |= data << 16;

            if data & (1 << 7) == 0 {
                out_buf.push(Insn::from_word(data)?);
                continue;
            }

            // otherwise, read next bytes
            data &= 0x0000_ffff;
            data |= (*values.next()? as u32) << 16;
            data |= (*values.next()? as u32) << 24;
            out_buf.push(Insn::from_word(data)?);
        }

        Some(out_buf)
    }

    pub fn length(&self) -> usize {
        match &self.content {
            InsnContent::Tiny {..} | InsnContent::Short {..} => 2,
            _ => 4
        }
    }
}


