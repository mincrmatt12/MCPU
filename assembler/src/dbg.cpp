#include "dbg.h"
#include <bitset>

namespace masm::dbg {
	void dump_expr(const masm::parser::expr &e) {
		switch (e.type) {
			case masm::parser::expr::num:
				std::cout << e.constant_value;
				break;
			case masm::parser::expr::label:
				std::cout << std::dec << "l" << e.label_value.section << "i" << e.label_value.index;
				break;
			case masm::parser::expr::undef:
				std::cout << "UNDEF";
				break;
			case masm::parser::expr::add:
				std::cout << "(";
				dump_expr(e.args[0]);
				std::cout << " + ";
				dump_expr(e.args[1]);
				std::cout << ")";
				break;
			case masm::parser::expr::sub:
				std::cout << "(";
				dump_expr(e.args[0]);
				std::cout << " - ";
				dump_expr(e.args[1]);
				std::cout << ")";
				break;
			case masm::parser::expr::mul:
				std::cout << "(";
				dump_expr(e.args[0]);
				std::cout << " * ";
				dump_expr(e.args[1]);
				std::cout << ")";
				break;
			case masm::parser::expr::div:
				std::cout << "(";
				dump_expr(e.args[0]);
				std::cout << " / ";
				dump_expr(e.args[1]);
				std::cout << ")";
				break;
			case masm::parser::expr::mod:
				std::cout << "(";
				dump_expr(e.args[0]);
				std::cout << " % ";
				dump_expr(e.args[1]);
				std::cout << ")";
				break;
			case masm::parser::expr::lshift:
				std::cout << "(";
				dump_expr(e.args[0]);
				std::cout << " << ";
				dump_expr(e.args[1]);
				std::cout << ")";
				break;
			case masm::parser::expr::rshift:
				std::cout << "(";
				dump_expr(e.args[0]);
				std::cout << " >> ";
				dump_expr(e.args[1]);
				std::cout << ")";
				break;
			case masm::parser::expr::neg:
				std::cout << "-(";
				dump_expr(e.args[0]);
				std::cout << ")";
				break;
		}
	}

	void dump_iarg(const masm::parser::insn_arg &arg) {
		std::cout << ", ";
		switch (arg.mode) {
			case masm::parser::insn_arg::UNDEFINED:
				std::cout << "aUNDEF";
				break;
			case masm::parser::insn_arg::REGISTER:
				std::cout << "r" << std::dec << arg.reg;
				break;
			case masm::parser::insn_arg::REGISTER_PLUS:
			case masm::parser::insn_arg::REGISTER_LSHIFT:
			case masm::parser::insn_arg::REGISTER_RSHIFT:
				std::cout << "r" << std::dec << arg.reg << " ";
				if (arg.mode == masm::parser::insn_arg::REGISTER_PLUS) {
					std::cout << "+ ";
					dump_expr(arg.constant);
				}
				else {
					std::cout << (arg.mode == masm::parser::insn_arg::REGISTER_LSHIFT ? "<< " : ">> ") << int{arg.shift};
				}
				break;
			case masm::parser::insn_arg::CONSTANT:
				dump_expr(arg.constant);
				break;
		}
	}

	void dump_pctx(const parser::pctx &pctx) {
		for (const auto& section : pctx.sections) {
			std::cout << "=== section " << section.index << " ===\n";
			std::cout << "starts at: " << std::hex;
			dump_expr(section.starting_address);
			std::cout << std::dec << "\n";
			std::cout << "insns:\n";
			for (const auto& insn : section.instructions) {
				switch (insn.type) {
					case masm::parser::insn::LOADSTORE:
						std::cout << "  ls{K=" << insn.i_ls.kind << ",S=" << insn.i_ls.size << ",TT=" << std::bitset<2>(insn.i_ls.dest) << "}";
						dump_iarg(insn.args[0]);
						std::cout << ", [";
						dump_expr(insn.addr.constant);
						std::cout << " + r" << std::dec << insn.addr.reg_base << " + r" << insn.addr.reg_index << " << " << int{insn.addr.shift} << "]";
						break;
					case masm::parser::insn::ALU:
						std::cout << "  alu{OOOO=" << std::bitset<4>(insn.i_alu) << "}";
						for (const auto& iarg : insn.args) dump_iarg(iarg);
						break;
					case masm::parser::insn::MOV:
						std::cout << "  " << (insn.i_mov.is_jmp ? "jmp" : "mov") << "{c=" << insn.i_mov.condition_string() << "}";
						for (const auto& iarg : insn.args) dump_iarg(iarg);
						break;
					case masm::parser::insn::UNDEFINED:
						std::cout << "  undef";
						break;
					case masm::parser::insn::LABEL:
						std::cout << "l" << insn.lbl.section << "i" << insn.lbl.index << ":";
						break;
				}
				std::cout << "\n";
			}
		}
	}
}
