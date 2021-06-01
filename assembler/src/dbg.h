#ifndef MCASM_DBG
#define MCASM_DBG

#include <parser.h>
#include <insns.h>
#include <ostream>
#include "layt.h"

std::ostream& operator<<(std::ostream& os, const masm::parser::expr &e);
std::ostream& operator<<(std::ostream& os, const masm::parser::insn_arg &arg);
std::ostream& operator<<(std::ostream& os, const masm::parser::pctx &pctx);
std::ostream& operator<<(std::ostream& os, const masm::layt::lctx &pctx);

void report_error(const masm::parser::pctx& ctx, const yy::location &l, const std::string &m);
extern bool is_error_reported_yet;

#endif
