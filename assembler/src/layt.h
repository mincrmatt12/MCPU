#pragma once

#include <parser.h>
#include "eval.h"
#include "insns.h"
#include <numeric>

extern void report_error(const masm::parser::pctx& ctx, const yy::location &l, const std::string &m);

namespace masm::layt {
	struct concreteinsn {
		// separate representation of an instruction. still uses parser::expr but everything
		// else is resolved to actual fields in the instruction encoding.

		enum t {
			DATA,
			INSN,
			UNDEF = -1
		} type = UNDEF;

		// proxy
		parser::rawdata d_data;

		// instruction type
		enum st {
			I_UNDEF = -1,
			I_SHORT,
			I_TINY,
			I_LONG,
			I_BIG,
			I_MED,
			I_MSM,
			I_SM
		} i_subtype = I_UNDEF;

		// instruction encoding

		// instruction opcode
		uint32_t opcode;
		// components
		uint32_t rd, rs, ro;
		uint32_t FF;
		// immediate
		parser::expr imm;

		// length in bytes (most useful for cpu addressing)
		size_t length() const {
			switch (type) {
				default:
					return 0;

				case DATA:
					switch (d_data.type) {
						case parser::rawdata::BYTES:
						case parser::rawdata::WORD:
							return 2;
						case parser::rawdata::DOUBLEWORD:
							return 4;
						case parser::rawdata::QUADWORD:
							return 8;
					}

				case INSN:
					switch (i_subtype) {
						case I_UNDEF:
							return 0;

						case I_SHORT:
						case I_TINY:
							return 2;
					
						default:
							return 4;
					}
			}
		}

		yy::location progpos;
	};

	struct layoutsection {
		// layed out section removes labels and fills up an evaluator
		uint32_t base_address = 0;
		size_t index = ~0ul;

		std::vector<concreteinsn> contents;

		size_t length() const {
			return std::accumulate(contents.cbegin(), contents.cend(), (size_t)0, [&](size_t x, const auto& v){return x + v.length();});
		}
	};

	struct lctx {
		std::vector<layoutsection> sections;
		eval::evaluator &evalt;

		lctx(eval::evaluator &evalt) : evalt(evalt) {}
		
		// Layout the parsed data, loading labels into the evaluator.
		bool layout_from(parser::pctx &pctx) {
			bool ok = true;
			// Layout each section
			for (auto& section : pctx.sections) {
				// Create new empty layoutsection
				sections.emplace_back();
				// Copy properties
				current().index = section.index;
				current().base_address = evalt.completely_evaluate<uint32_t>(section.starting_address);

				// Keep track of current address
				uint32_t addr = current().base_address;

				// Start parsing instructions
				for (auto& insn : section.instructions) {
					// Is this a label?
					if (insn.type == parser::insn::LABEL) {
						// Set the label's address
						evalt.labelvalues[insn.lbl] = parser::expr(addr); // widening conversion works fine here
					}
					else {
						try {
							// Otherwise, layout
							layout_instruction(std::move(insn));
						}
						catch (std::domain_error &e) {
							ok = false;
							::report_error(pctx, insn.progpos, e.what());
						}
						// Increment counter
						addr += currenti().length();
					}
				}
			}
			// Detect overlaps (by first sorting)
			std::sort(sections.begin(), sections.end(), [&](const auto& x, const auto& y){return x.base_address < y.base_address;});
			for (int i = 0; i < sections.size()-1; ++i) {
				if (sections[i].base_address + sections[i].length() > sections[i+1].base_address) {
					ok = false;
					char buf[256];
					snprintf(buf, 256, "overlapping sections: (0x%08x + %zx > 0x%08x)", sections[i].base_address, sections[i].length(), sections[i+1].base_address);
					::report_error(pctx, sections[i].contents.back().progpos, std::string{buf});
				}
			}

			return ok;
		}

	private:
		void layout_instruction(parser::insn &&insn) {
			// Create a new instruction
			current().contents.emplace_back();
			// Forward position in source
			currenti().progpos = insn.progpos;

			// Forward data
			if (insn.type == parser::insn::DATA) {
				currenti().type = concreteinsn::DATA;
				currenti().d_data = std::move(insn.raw);
			}
			else {
				currenti().type = concreteinsn::INSN;
				// Otherwise, try to assemble something.
				if (insn.type == parser::insn::LOADSTORE) {
					// Get the rD
					currenti().rd = insn.args[0].reg;
					// Try to use short encoding:
					if (insn.addr.reg_index == 0 && insn.addr.constant.is_constant(0)) {
						currenti().i_subtype = concreteinsn::I_SHORT;
						currenti().ro = insn.addr.reg_base;
						currenti().opcode = insn::build_load_store_opcode(
							insn.i_ls.kind, insn.i_ls.size, insn.i_ls.dest, insn::load_store_address_mode::GENERIC
						);
					}
					else {
						// Otherwise, check which mode we have to use.
						//
						// If there is no index register, we should always use the simple mode as it gives more flexibility with the constant
						if (insn.addr.reg_index == 0 && insn.addr.constant.type == parser::expr::num) {
							currenti().i_subtype = concreteinsn::I_MSM;
							currenti().ro = insn.addr.reg_base;
							currenti().opcode = insn::build_load_store_opcode(
								insn.i_ls.kind, insn.i_ls.size, insn.i_ls.dest, insn::load_store_address_mode::SIMPLE
							);

							if (insn.addr.constant.constant_value > 0xffff'ffff) {
								throw std::domain_error("invalid address: greater than 32bits");
							}
							
							int64_t base_constant = insn.addr.constant.constant_value & ~(0b11 << 30);
							int64_t ff = (insn.addr.constant.constant_value >> 30) & 0b11;
							base_constant |= ((base_constant & (1 << 29)) ? 0b11 : 0) << 30;
							currenti().imm = parser::expr(base_constant);
							currenti().FF = ff;
						}
						else {
							currenti().i_subtype = concreteinsn::I_SM;
							currenti().ro = insn.addr.reg_base;
							currenti().rs = insn.addr.reg_index;
							currenti().opcode = insn::build_load_store_opcode(
								insn.i_ls.kind, insn.i_ls.size, insn.i_ls.dest, insn::load_store_address_mode::GENERIC
							);
							currenti().FF = insn.addr.shift;
							currenti().imm = insn.addr.constant;
						}
					}
					
				}
				else if (insn.type == parser::insn::ALU) {
					// Get the rD
					currenti().rd = insn.args[0].reg;

					// If the second argument has a shift, we have to use the complex sm encoding
					if (
						insn.args[2].mode == parser::insn_arg::REGISTER_LSHIFT || insn.args[2].mode == parser::insn_arg::REGISTER_RSHIFT
					) {
						currenti().i_subtype = concreteinsn::I_SM;
						currenti().rs = insn.args[1].reg;
						currenti().ro = insn.args[2].reg;
						currenti().FF = insn.args[2].shift - 1;
						currenti().opcode = insn::build_alu_opcode(
							insn.i_alu, insn.args[2].mode == parser::insn_arg::REGISTER_LSHIFT ? insn::alu_sty::REGSL : insn::alu_sty::REGSR
						);
					}
					else {
						// Otherwise, can we use a short encoding?
						// There are two styles that support it, reg = reg @ reg and reg = reg @ imm
						// Check for the first case
						if (insn.args[2].mode == parser::insn_arg::REGISTER && insn.args[0].reg == insn.args[1].reg) {
							// use a short encoding
							currenti().i_subtype = concreteinsn::I_SHORT;
							currenti().ro = insn.args[2].reg;
							currenti().opcode = insn::build_alu_opcode(
								insn.i_alu, insn::alu_sty::REG
							);
						}
						else if (insn.args[2].mode == parser::insn_arg::CONSTANT && insn.args[0].reg == insn.args[1].reg && insn.args[2].constant.type == parser::expr::num &&
								 insn::fits(insn.args[2].constant.constant_value, 4)) {
							// use timm short encoding
							currenti().i_subtype = concreteinsn::I_TINY;
							currenti().rs = insn.args[1].reg; // not strictly required b.c. short encoding but makes debug output better
							currenti().imm = insn.args[2].constant;
							currenti().opcode = insn::build_alu_opcode(
								insn.i_alu, insn::alu_sty::IMM
							);
						}
						else {
							// is this a register-register?
							if (insn.args[2].mode == parser::insn_arg::REGISTER) {
								currenti().i_subtype = concreteinsn::I_LONG;
								currenti().rs = insn.args[1].reg;
								currenti().ro = insn.args[2].reg;
								currenti().opcode = insn::build_alu_opcode(
									insn.i_alu, insn::alu_sty::REG
								);
							}
							// it's an immediate
							else {
								currenti().i_subtype = concreteinsn::I_MED;
								currenti().ro = insn.args[1].reg;
								currenti().imm = insn.args[2].constant;
								currenti().opcode = insn::build_alu_opcode(
									insn.i_alu, insn::alu_sty::IMM
								);
							}
						}
					}
				}
				else if (insn.type == parser::insn::MOV) {
					auto cond = insn.i_mov.condition;
					auto inscond = insn::mov_cond::AL;
					// before doing anything, perform condition replacement
					if (insn.i_mov.needs_swap(cond)) std::swap(insn.args[insn.args.size()-2], insn.args[insn.args.size()-1]);
					// extract new condition 
					switch (cond) {
						case parser::mov_insn::AL:
						default:
							break;
						case parser::mov_insn::BS:
							inscond = insn::mov_cond::BS;
							break;
						case parser::mov_insn::GE:
							inscond = insn::mov_cond::GE;
							break;
						case parser::mov_insn::LT:
							inscond = insn::mov_cond::LT;
							break;
						case parser::mov_insn::SGE:
							inscond = insn::mov_cond::SGE;
							break;
						case parser::mov_insn::SLT:
							inscond = insn::mov_cond::SLT;
							break;
						case parser::mov_insn::EQ:
							inscond = insn::mov_cond::EQ;
							break;
						case parser::mov_insn::NE:
							inscond = insn::mov_cond::NEQ;
							break;
					}
					// additionally, try to replace constant-zeroes with reg-zeroes, since they will work in more places [citation needed]
					if (insn.args.size() > 2) {
						for (int i = insn.args.size() - 2; i < insn.args.size(); ++i) {
							if (insn.args[i].mode == parser::insn_arg::CONSTANT && insn.args[i].constant.is_constant(0)) {
								insn.args[i].mode = parser::insn_arg::REGISTER;
								insn.args[i].reg = 0;
							}
						}
					}
					// assemble jumps separately
					if (insn.i_mov.is_jmp) {
						// handle two special cases:
						//   if jump target is a register with no condition, use short encoding
						if (insn.args[0].mode == parser::insn_arg::REGISTER && insn.i_mov.condition == parser::mov_insn::AL) {
							currenti().i_subtype = concreteinsn::I_SHORT;
							currenti().rd = 0b1111;
							currenti().ro = insn.args[0].reg;
							currenti().opcode = insn::build_mov_opcode(
								insn::mov_op::MRO, insn::mov_cond::AL
							);
						}
						//  if jump target is an immediate with no condition, use short encoding
						else if (insn.args[0].mode == parser::insn_arg::CONSTANT  && insn.i_mov.condition == parser::mov_insn::AL) {
							currenti().i_subtype = concreteinsn::I_BIG;
							currenti().rd = 0b1111;
							currenti().imm = insn.args[0].constant;
							currenti().opcode = insn::build_mov_opcode(
								insn::mov_op::MIMM, insn::mov_cond::AL
							);

							// if the immediate certainly fits in a short encoding, use A
							if (insn.args[0].constant.type == parser::expr::num && insn::fits(insn.args[0].constant.constant_value, 4)) {
								currenti().i_subtype = concreteinsn::I_TINY;
							}
						}
						else {
							// otherwise try to assemble a jump:
							//   encoding is always T
							currenti().i_subtype = concreteinsn::I_SM;
							currenti().FF = 0;
						
							// deduce FF + check for errors
							if (insn.args[0].mode == parser::insn_arg::REGISTER_PLUS) {
								// jump with FF = 11; make sure args 1/2 (if present) are registers
								if (insn.args.size() == 3 && std::any_of(insn.args.begin() + 1, insn.args.end(), [](const auto& x){return x.mode != parser::insn_arg::REGISTER;})) {
									throw std::domain_error("invalid use of jmp: target is reg + const but at least one condition is not register");
								}
								currenti().FF = 0b11;
								currenti().imm = insn.args[0].constant;
							}
							// check for op1/op2
							else if (insn.args.size() == 3) {
								for (int i = 1; i < 3; ++i) {
									if (insn.args[i].mode == parser::insn_arg::CONSTANT) {
										if (currenti().FF) {
											throw std::domain_error("invalid use of jmp: condition args cannot both be immediates");
										}
										else {
											// setup FF for replace
											currenti().FF = 1 << (i-1);
											currenti().imm = insn.args[i].constant;
										}
									}
								}
							}
							// error checking is OK, slot in registers/condition + generate opcode
							currenti().opcode = insn::build_mov_opcode(insn::mov_op::JUMP, inscond);
							// setup registers
							currenti().rd = insn.args[0].reg;
							currenti().rs = insn.args[1].reg;
							currenti().ro = insn.args[2].reg;
						}
					}
					else {
						// Try to assemble a mov instead. The easiest mov rules to implement are the load-immediate ones, so try them first
						if (insn.args[1].mode == parser::insn_arg::CONSTANT) {
							// If the condition is always, try the B and shrink if it fits
							if (insn.i_mov.condition == parser::mov_insn::AL) {
								currenti().opcode = insn::build_mov_opcode(insn::mov_op::MIMM, insn::mov_cond::AL);
								currenti().rd = insn.args[0].reg;
								currenti().imm = insn.args[1].constant;
								currenti().i_subtype = concreteinsn::I_BIG;

								// if the immediate certainly fits in a short encoding, use A
								if (insn.args[1].constant.type == parser::expr::num && insn::fits(insn.args[1].constant.constant_value, 4)) {
									currenti().i_subtype = concreteinsn::I_TINY;
								}
							}
							// Otherwise, use L encoding
							else {
								// ensure other args are registesr
								if (std::any_of(insn.args.begin() + 2, insn.args.end(), [](const auto& x){return x.mode != parser::insn_arg::REGISTER;})) {
									throw std::domain_error("invalid use of mov-immediate: condition args must be plain registers.");
								}
								currenti().opcode = insn::build_mov_opcode(insn::mov_op::MIMM, inscond);
								currenti().i_subtype = concreteinsn::I_LONG;
								currenti().rd = insn.args[0].reg;
								currenti().imm = insn.args[1].constant;
								currenti().rs = insn.args[2].reg;
								currenti().ro = insn.args[3].reg;
							}
						}
						// Otherwise, the encoding is [T]. 
						else {
							// There are a few approaches we can use here, based on the various types that are allowed.
							//
							// insn.args[0] is always a REGISTER (and is free to be whatever)
							// insn.args[1] must be one of:
							//   - insn.args[2] or insn.args[3] (in which case we use FF = 00 and raw LOP1/2
							//   - any register, as long as insn.args[2] XOR insn.args[3] is an immediate (in which case we use FF = REPL1/2 and LOP1/2, using the now 
							//                                                                             unused operand as the target to load)
							//   - insn.args[2] or insn.args[3] +/- some constant (in which case we use FF = 11)
							//
							// There is also a special case for reg = reg, since they use short-encoding. We check that first.
							if (insn.args[1].mode == parser::insn_arg::REGISTER && insn.i_mov.condition == parser::mov_insn::AL) {
								// Assemble a short-mode encoding
								currenti().opcode = insn::build_mov_opcode(insn::mov_op::MRO, insn::mov_cond::AL);
								currenti().i_subtype = concreteinsn::I_SHORT;
								currenti().ro = insn.args[1].reg;
								currenti().rd = insn.args[0].reg;
								currenti().rs = insn.args[0].reg;
							}
							// Next, check for the second case
							else if (auto x = std::count_if(insn.args.begin() + 2, insn.args.end(), [](const auto& y){return y.mode == parser::insn_arg::CONSTANT;}); x >= 1) {
								if (x == 2) {
									throw std::domain_error("invalid use of mov-reg: only one param can be immediate.");
								}

								// ensure target is a register
								if (insn.args[1].mode != parser::insn_arg::REGISTER) {
									throw std::domain_error("invalid use of mov-reg: must be moving raw register with immediate comparison");
								}

								// find spare operand
								bool spare = (insn.args[2].mode == parser::insn_arg::CONSTANT); // true if spare is in operand 0
								currenti().opcode = insn::build_mov_opcode(spare ? insn::mov_op::MRS : insn::mov_op::MRO, inscond);
								currenti().i_subtype = concreteinsn::I_SM;
								// setup register
								(spare ? currenti().rs : currenti().ro) = insn.args[1].reg;
								// setup register condition
								(spare ? currenti().ro : currenti().rs) = insn.args[spare ? 3 : 2].reg;
								// setup immediate
								currenti().imm = insn.args[spare ? 2 : 3].constant;
								currenti().rd = insn.args[0].reg;
								// Pick FF based on spare
								currenti().FF = spare ? 0b01 : 0b10;
							}
							// At this point, we know for a fact there are no constants involved (other than for +/-). Ensure there's a correspondence _somewhere_
							else if (auto x = std::find_if(insn.args.begin() + 2, insn.args.end(), [&](const auto& y){return y.reg == insn.args[1].reg;}); x != insn.args.end()) {
								bool pos = x == insn.args.begin() + 2; // true if LOP1/MRS
								// Setup registers
								currenti().rd = insn.args[0].reg;
								currenti().rs = insn.args[1].reg;
								currenti().ro = insn.args[2].reg;
								// Calculate opcode
								currenti().opcode = insn::build_mov_opcode(pos ? insn::mov_op::MRS : insn::mov_op::MRO, inscond);
								// Setup size
								currenti().i_subtype = concreteinsn::I_SM;
								// Check if we need to fill-in immediate
								if (insn.args[1].mode == parser::insn_arg::REGISTER_PLUS) {
									currenti().imm = insn.args[1].constant;
									currenti().FF = 0b11;
								}
								else {
									currenti().FF = 0b00;
								}
							}
							else {
								throw std::domain_error("invalid use of mov-reg: must have a register re-use to target lop1/2.");
							}
						}
					}
				}
			}
		}

		layoutsection& current() {
			return sections.back();
		}

		concreteinsn& currenti() {
			return sections.back().contents.back();
		}

		
	};
};
