// Currently just a basic dissassembler
mod insn;

use std::env;
use std::fs;

fn main() {
    // read input
    let args: Vec<String> = env::args().collect();
    let raw_data = fs::read(&args[1]).expect("unable to read input");
    // Try to dissassemble stuff

    let insns = insn::Insn::dissasemble(
        (&raw_data[0x200..0x22e]).iter()
    ).expect("invalid bytes");

    let mut curraddr = 0x200;

    for i in &insns {
        println!("{:08x}: {}", curraddr, i);
        curraddr += i.length();
    }
}
