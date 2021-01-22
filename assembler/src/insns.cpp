#include "insns.h"

namespace masm {
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
};
