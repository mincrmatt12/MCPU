#include "dbg.h"
#include <bitset>
#include <ranges>

bool is_error_reported_yet = false;

namespace {
	void dump(std::ostream &os, const std::ranges::range auto& obj, const char *sep) {
		bool f = false;
		for (const auto& x : obj) {
			if (f) os << sep;
			os << x;
			f = true;
		}
	}
}

std::ostream& operator<<(std::ostream& os, const masm::parser::expr &e) {
	auto flgs = os.flags();
	switch (e.type) {
		case masm::parser::expr::num:
			os << e.constant_value;
			break;
		case masm::parser::expr::label:
			os << std::dec << "l" << e.label_value.section << "i" << e.label_value.index;
			break;
		case masm::parser::expr::undef:
			os << "UNDEF";
			break;
		case masm::parser::expr::add:
			os << "(";
			dump(os, e.args, " + ");
			os << ")";
			break;
		case masm::parser::expr::mul:
			os << "(";
			dump(os, e.args, " * ");
			os << ")";
			break;
		case masm::parser::expr::div:
			os << "(";
			dump(os, e.args, " / ");
			os << ")";
			break;
		case masm::parser::expr::mod:
			os << "(";
			dump(os, e.args, " % ");
			os << ")";
			break;
		case masm::parser::expr::lshift:
			os << "(";
			dump(os, e.args, " << ");
			os << ")";
			break;
		case masm::parser::expr::rshift:
			os << "(";
			dump(os, e.args, " >> ");
			os << ")";
			break;
		case masm::parser::expr::neg:
			os << "-(" << e.args.front() << ")";
			break;
	}
	os.flags(flgs);
	return os;
}

std::ostream& operator<<(std::ostream& os, const masm::parser::insn_arg &arg) {
	switch (arg.mode) {
		case masm::parser::insn_arg::UNDEFINED:
			os << "aUNDEF";
			break;
		case masm::parser::insn_arg::REGISTER:
			os << "r" << std::dec << arg.reg;
			break;
		case masm::parser::insn_arg::REGISTER_PLUS:
		case masm::parser::insn_arg::REGISTER_LSHIFT:
		case masm::parser::insn_arg::REGISTER_RSHIFT:
			os << "r" << std::dec << arg.reg << " ";
			if (arg.mode == masm::parser::insn_arg::REGISTER_PLUS) {
				os << "+ " << arg.constant;
			}
			else {
				os << (arg.mode == masm::parser::insn_arg::REGISTER_LSHIFT ? "<< " : ">> ") << int{arg.shift};
			}
			break;
		case masm::parser::insn_arg::CONSTANT:
			os << arg.constant;
			break;
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const masm::parser::pctx &pctx) {
	for (const auto& section : pctx.sections) {
		os << "=== section " << section.index << " ===\n";
		os << "starts at: " << std::hex << "0x" << section.starting_address << std::dec << "\n";
		os << "insns:\n";
		for (const auto& insn : section.instructions) {
			switch (insn.type) {
				case masm::parser::insn::LOADSTORE:
					os << "  ls{K=" << insn.i_ls.kind << ",S=" << insn.i_ls.size << ",TT=" << std::bitset<2>(insn.i_ls.dest) << "}, " << insn.args[0] << ", [0x" << std::hex << insn.addr.constant;
					os << " + r" << std::dec << insn.addr.reg_base << " + r" << insn.addr.reg_index << " << " << int{insn.addr.shift} << "]";
					break;
				case masm::parser::insn::ALU:
					os << "  alu{OOOO=" << std::bitset<4>(insn.i_alu) << "}, ";
					dump(os, insn.args, ", ");
					break;
				case masm::parser::insn::MOV:
					os << "  " << (insn.i_mov.is_jmp ? "jmp" : "mov") << "{c=" << insn.i_mov.condition_string() << "}, ";
					dump(os, insn.args, ", ");
					break;
				case masm::parser::insn::UNDEFINED:
					os << "  undef";
					break;
				case masm::parser::insn::LABEL:
					os << "l" << insn.lbl.section << "i" << insn.lbl.index << ":";
					break;
				case masm::parser::insn::DATA:
					os << "  db{width=" << insn.raw.type << "}, 0x" << std::hex << insn.raw.low;
					if (insn.raw.type == masm::parser::rawdata::BYTES) os << ", 0x" << insn.raw.high;
					os << std::dec;
					break;
			}
			os << "\n";
		}
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const masm::layt::lctx &lctx) {
	for (const auto& section : lctx.sections) {
		os << "=== layed out section " << section.index << " ===\n";
		os << "base address: " << std::hex << "0x" << section.base_address << std::dec << "\n";
		os << "contents:\n";
		uint32_t addr = section.base_address;
		for (const auto& insn : section.contents) {
			os << std::hex << std::setw(10) << addr << std::dec << std::setw(0) << ": ";
			switch (insn.type) {
				case masm::layt::concreteinsn::DATA:
					os << "db{width=" << insn.d_data.type << "}, 0x" << std::hex << insn.d_data.low;
					if (insn.d_data.type == masm::parser::rawdata::BYTES) os << ", 0x" << insn.d_data.high;
					os << std::dec;
					break;
				case masm::layt::concreteinsn::INSN:
					os << "opc=" << std::bitset<7>(insn.opcode) << ", rd=" << insn.rd;
					switch (insn.i_subtype) {
					case masm::layt::concreteinsn::I_SHORT:
						os << ", rs=" << insn.rs << ", ro=" << insn.ro;
						break;
					case masm::layt::concreteinsn::I_TINY:
						os << ", rs=" << insn.rs << ", imm=" << std::hex << insn.imm << std::dec;
						break;
					case masm::layt::concreteinsn::I_SM:
					case masm::layt::concreteinsn::I_LONG:
						os << ", rs=" << insn.rs;
					case masm::layt::concreteinsn::I_MSM:
						if (insn.i_subtype != masm::layt::concreteinsn::I_LONG) os << ", FF=" << std::bitset<2>(insn.FF);
					case masm::layt::concreteinsn::I_MED:
						os << ", ro=" << insn.ro;
					case masm::layt::concreteinsn::I_BIG:
						os << ", imm=" << std::hex << insn.imm << std::dec;
					default:
						break;
					}
					break;
				default:
					os << "<undef>";
					break;
			}
			addr += insn.length();
			os << "\n";
		}
	}
	return os;
}

void report_error(const masm::parser::pctx& ctx, const yy::location &l, const std::string &m) {
	std::cerr << (l.begin.filename ? l.begin.filename->c_str() : "(undefined)");
	std::cerr << ':' << l.begin.line << ':' << l.begin.column << '-' << l.end.column << ": " << m << '\n';

	try {
		const char * line = ctx.start + ctx.lineoffsets.at(l.begin.line - 1);
		std::cerr << std::setw(6) << std::right << l.begin.line << " | ";
		while (*line != '\n') {
			std::cerr << *line++;
		}
		std::cerr << "\n         ";
		for (size_t i = 0; i < l.begin.column - 1; ++i) {
			std::cerr << ' ';
		}
		for (size_t i = 0; i < (l.end.column - l.begin.column); ++i) {
			std::cerr << (i == 0 ? '^' : '~');
		}
		std::cerr << "\n";
	}
	catch (const std::out_of_range &e) {}
	is_error_reported_yet = true;
}
