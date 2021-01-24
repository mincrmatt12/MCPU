#include "insns.h"
#include <parser.h>
#include <iostream>
#include <fstream>
#include "dbg.h"

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

	if (!parser.parse()) {
		return 1;
	}

	// DEBUG: dump insn
	masm::dbg::dump_pctx(pctx);
}
