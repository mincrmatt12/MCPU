#include "insns.h"
#include <parser.h>
#include <iostream>
#include <fstream>
#include "dbg.h"
#include "eval.h"

int main(int argc, char ** argv) {
	std::string f_data;
	std::string f_name = argv[1];

	{
		std::ifstream f_in(f_name);
		f_in.seekg(0, std::ios::end);
		f_data.resize(f_in.tellg());
		f_in.seekg(0, std::ios::beg);
		f_in.read(f_data.data(), f_data.size());
	}

	// parse
	
	masm::parser::pctx pctx;
	auto parser = yy::mcasm_parser(pctx);

	pctx.prepare_cursor(f_data.data());
	pctx.loc.begin.filename = &f_name;
	pctx.loc.end.filename = &f_name;

	if (parser.parse()) {
		return 1;
	}

	// DEBUG: dump insn
	std::cout << pctx;

	masm::eval::evaluator eval;

	// simplify expressions
	for (auto& section : pctx.sections) {
		for (auto& insn : section.instructions) {
			switch (insn.type) {
				case masm::parser::insn::LOADSTORE:
					eval.simplify(insn.addr.constant);
					[[fallthrough]];
				case masm::parser::insn::ALU:
				case masm::parser::insn::MOV:
					for (auto& arg : insn.args) {
						switch (arg.mode) {
							case masm::parser::insn_arg::CONSTANT:
							case masm::parser::insn_arg::REGISTER_PLUS:
								// simplify + process
								eval.simplify(arg.constant);
							default:
								break;
						}
					}
					break;
				case masm::parser::insn::DATA:
					eval.simplify(insn.raw.low);
					if (insn.raw.type == masm::parser::rawdata::BYTES) {
						eval.simplify(insn.raw.high);
					}
				default:
					break;
			}
		}
	}

	// show evaluated debug
	std::cout << "after eval:\n" << pctx << "\n";
	
	// layout memory / pick opcodes
	
	// assemble
	
	// write to binary
}
