// Currently just a basic dissassembler
mod insn;

use std::env;
use std::fs;

fn show_disas(insns: Vec<insn::Insn>, start_addr: u32) {
    let mut curraddr: usize = start_addr as usize;

    for i in &insns {
        println!("{:08x}: {}", curraddr, i);
        curraddr += i.length();
    }
}

fn disas(raw_data: Vec<u8>) {
    // Try to dissassemble stuff

    let mut ptr = 0;
    while ptr < raw_data.len() {
        let start_addr: u32 = 
            (raw_data[ptr] as u32) |
            ((raw_data[ptr + 1] as u32) << 8) |
            ((raw_data[ptr + 2] as u32) << 8) |
            ((raw_data[ptr + 3] as u32) << 8);

        ptr += 4;

        let section_length: usize = 
            (raw_data[ptr] as usize) |
            ((raw_data[ptr + 1] as usize) << 8) |
            ((raw_data[ptr + 2] as usize) << 8) |
            ((raw_data[ptr + 3] as usize) << 8);

        ptr += 4;

        println!("\nsection at 0x{:08x}; length {:08x}\n", start_addr, section_length);

        let insns = insn::Insn::dissasemble(
            (&raw_data[ptr..(ptr+section_length)]).iter()
        );

        match insns {
            Some(x) => show_disas(x, start_addr),
            _ => println!("<invalid contents>")
        }

        ptr += section_length;
    }
}

fn main() {
    // read input
    let args: Vec<String> = env::args().collect();

    let raw_data = fs::read(&args[2]).expect("unable to read input");

    match args[1].as_str() {
        "dis" | "dissassemble" => {
            disas(raw_data)
        }
        _ => {
            println!("Unknown command");
            return
        }
    }
}
