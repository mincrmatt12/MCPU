#ifndef MCPU_INSN
#define MCPU_INSN

#include <stdint.h>
#include <stdexcept>

namespace masm::insn {
	// Various enums for tables in the ISA

	namespace mov_cond {
		enum mov_cond : uint32_t {
			LT = 0b000,
			SLT = 0b001,
			GE = 0b010,
			SGE = 0b011,
			EQ = 0b100,
			NEQ = 0b101,
			BS = 0b110,
			AL = 0b111,
		};

		using e = mov_cond;
	}

	namespace mov_op {
		enum mov_op : uint32_t {
			MIMM = 0b00,
			JUMP = 0b01,
			MRS = 0b10,
			MRO = 0b11
		};

		using e = mov_op;
	}

	namespace alu_op {
		enum alu_op : uint32_t {
			ADD  = 0b0000,
			SUB  = 0b0001,
			SL   = 0b0010,
			SR   = 0b0011,
			LSL  = 0b0100,
			LSR  = 0b0101,
			OR   = 0b1000,
			EOR  = 0b1001,
			AND  = 0b1010,
			NOR  = 0b1100,
			ENOR = 0b1101,
			NAND = 0b1110
		};

		using e = alu_op;
	}

	namespace alu_sty {
		enum alu_sty : uint32_t {
			REG = 0b00,
			IMM = 0b01,
			REGSL = 0b10,
			REGSR = 0b11
		};

		using e = alu_sty;
	}

	namespace load_store_dest {
		enum load_store_dest : uint32_t {
			ZEXT = 0b00,
			SEXT = 0b01,
			LOWW = 0b10,
			HIGHW = 0b11
		};

		using e = load_store_dest;
	}

	namespace load_store_address_mode {
		enum load_store_address_mode : uint32_t {
			GENERIC = 0,
			SIMPLE = 1
		};

		using e = load_store_address_mode;
	}
	
	namespace load_store_size {
		enum load_store_size : uint32_t {
			BYTE = 0,
			HALFWORD = 1
		};

		using e = load_store_size;
	}

	namespace load_store_kind {
		enum load_store_kind : uint32_t {
			LOAD = 0,
			STORE = 1
		};

		using e = load_store_kind;
	}

	uint32_t build_load_store_opcode(load_store_kind::e kind, load_store_size::e size, load_store_dest::e dest, load_store_address_mode::e mode);
	uint32_t build_alu_opcode(alu_op::e op, alu_sty::e style);
	uint32_t build_mov_opcode(mov_op::e op, mov_cond::e cond);

	// Does value fit in bits, i.e. does SEXT(value[0:bits]) == value
	inline bool fits(uint32_t value, size_t bits) {
		uint32_t v_shift = (value >> bits);
		uint32_t negative = 0xffff'ffff >> bits;
		bool top_bit = value & (1 << (bits - 1));

		return (!top_bit && v_shift == 0) || (top_bit && v_shift == negative);
	}

	uint16_t build_short_insn(uint32_t rs_and_rd, uint32_t ro, uint32_t opcode);
	uint16_t build_timm_insn(uint32_t rs_and_rd, uint32_t imm, uint32_t opcode);
	uint32_t build_imm_insn(uint32_t rd, uint32_t imm, uint32_t rs, uint32_t ro, uint32_t opcode);
	uint32_t build_bigimm_insn(uint32_t rd, uint32_t imm, uint32_t opcode); 
	uint32_t build_mediimm_insn(uint32_t rd, uint32_t imm, uint32_t ro, uint32_t opcode);
	uint32_t build_msmimm_insn(uint32_t rd, uint32_t imm, uint32_t FF, uint32_t ro, uint32_t opcode);
	uint32_t build_smimm_insn(uint32_t rd, uint32_t imm, uint32_t FF, uint32_t rs, uint32_t ro, uint32_t opcode);
}

#endif
