#pragma once

#include "layt.h"

namespace masm::assmbl {
	void assemble(parser::pctx& pctx, layt::lctx &&lctx, std::ostream& os);
}
