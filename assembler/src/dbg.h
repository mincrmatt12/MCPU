#ifndef MCASM_DBG
#define MCASM_DBG

#include <parser.h>
#include <insns.h>
#include <ostream>

std::ostream& operator<<(std::ostream& os, const masm::parser::expr &e);
std::ostream& operator<<(std::ostream& os, const masm::parser::insn_arg &arg);
std::ostream& operator<<(std::ostream& os, const masm::parser::pctx &pctx);

#endif
