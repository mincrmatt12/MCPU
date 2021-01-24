#ifndef MCASM_DBG
#define MCASM_DBG

#include <parser.h>
#include <insns.h>

namespace masm::dbg {
	void dump_expr(const masm::parser::expr &e);
	void dump_iarg(const masm::parser::insn_arg &arg);
	void dump_pctx(const masm::parser::pctx &pctx);
}

#endif
