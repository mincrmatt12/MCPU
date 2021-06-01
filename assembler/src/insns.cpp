#include "insns.h"

namespace masm::insn {
	uint32_t build_load_store_opcode(load_store_kind::e kind, load_store_size::e size, load_store_dest::e dest, load_store_address_mode::e mode) {
		// Check if an invalid combination of dest/store was given
		if (kind == load_store_kind::STORE && !(dest & load_store_dest::LOWW)) {
			throw std::domain_error("invalid combination of load operations: zero/sign-extension for store");
		}

		return (kind << 4) | (size << 3) | (dest << 1) | mode;
	}

	uint32_t build_alu_opcode(alu_op::e op, alu_sty::e style) {
		return (1 << 6) | (op << 2) | style;
	}

	uint32_t build_mov_opcode(mov_op::e op, mov_cond::e cond) {
		return (0b01 << 5) | (cond << 2) | op;
	}

	void verify_register(uint32_t r) {
		if (r > 15) throw std::domain_error("invalid register number; must fit in 4 bits");
	}

	void verify_opcode(uint32_t opc) {
		if (opc > 127) throw std::domain_error("opcode must be 7bits");
	}

	void verify_ff(uint32_t ff) {
		if (ff > 3) throw std::domain_error("ff must be 0-3");
	}

	uint16_t build_short_insn(uint32_t rs_and_rd, uint32_t ro, uint32_t opcode) {
		verify_register(rs_and_rd);
		verify_register(ro);
		verify_opcode(opcode);

		return (rs_and_rd << 12) | (ro << 8) | opcode;
	}

	uint16_t build_timm_insn(uint32_t rs_and_rd, uint32_t imm, uint32_t opcode) {
		verify_register(rs_and_rd);
		verify_opcode(opcode);
		if (!fits(imm, 4)) throw std::domain_error("immediate in A instruction must be 4 bits or less");

		return (rs_and_rd << 12) | ((imm & 0b1111) << 8) | opcode;
	}

	uint32_t build_imm_insn(uint32_t rd, uint32_t imm, uint32_t rs, uint32_t ro, uint32_t opcode) {
		verify_register(rd);
		verify_register(rs);
		verify_register(ro);
		verify_opcode(opcode);
		if (!fits(imm, 12)) throw std::domain_error("immediate in L instruction must be 12 bits or less");
		imm &= (1u << 12) - 1;

		return (rd << 28) | (imm << 16) | (rs << 12) | (ro << 8) | (1 << 7) | opcode;
	}

	uint32_t build_bigimm_insn(uint32_t rd, uint32_t imm, uint32_t opcode) {
		verify_register(rd);
		//verify_register(rs);
		//verify_register(ro);
		verify_opcode(opcode);
		if (!fits(imm, 20)) throw std::domain_error("immediate in B instruction must be 20 bits or less");
		imm &= (1u << 20) - 1;

		return (rd << 28) | (imm << 8) | (1 << 7) | opcode;
	}

	uint32_t build_mediimm_insn(uint32_t rd, uint32_t imm, uint32_t ro, uint32_t opcode) {
		verify_register(rd);
		//verify_register(rs);
		verify_register(ro);
		verify_opcode(opcode);
		if (!fits(imm, 16)) throw std::domain_error("immediate in M instruction must be 16 bits or less");
		imm &= (1 << 16) - 1;

		return (rd << 28) | (imm << 12) | (ro << 8) | (1 << 7) | opcode;
	}

	uint32_t build_msmimm_insn(uint32_t rd, uint32_t imm, uint32_t FF, uint32_t ro, uint32_t opcode) {
		verify_register(rd);
		//verify_register(rs);
		verify_register(ro);
		verify_ff(FF);
		verify_opcode(opcode);
		if (!fits(imm, 14)) throw std::domain_error("immediate in F instruction must be 14 bits or less");
		imm &= (1 << 14) - 1;

		return (rd << 28) | (imm << 15) | (FF << 12) | (ro << 8) | (1 << 7) | opcode;
	}

	uint32_t build_smimm_insn(uint32_t rd, uint32_t imm, uint32_t FF, uint32_t rs, uint32_t ro, uint32_t opcode) {
		verify_register(rd);
		verify_register(rs);
		verify_register(ro);
		verify_ff(FF);
		verify_opcode(opcode);
		if (!fits(imm, 10)) throw std::domain_error("immediate in T instruction must be 10 bits or less");
		imm &= (1 << 10) - 1;

		return (rd << 28) | (imm << 18) | (FF << 16) | (rs << 12) | (ro << 8) | (1 << 7) | opcode;
	}
};
