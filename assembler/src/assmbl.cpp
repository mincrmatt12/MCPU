#include "assmbl.h"
#include "dbg.h"

void masm::assmbl::assemble(parser::pctx& pctx, layt::lctx &&lctx, std::ostream& os) {
	// TODO: do some sanity checks so we don't make huge files.
	
	auto put = [&](auto num){
		for (int i = 0; i < sizeof(num); ++i) {
			os.put(num & 0xff);
			num >>= 8;
		}
	};

	uint32_t current_address = 0x0;
	for (auto& section : lctx.sections) {
		// Generate 0x00 padding 
		while (current_address < section.base_address) {
			os.put(0);
			++current_address;
		}

		// Start assembling instructions
		for (auto& content : section.contents) {
			try {
				switch (content.type) {
					case layt::concreteinsn::DATA:
						// switch on type
						switch (content.d_data.type) {
							case parser::rawdata::BYTES:
								os.put(lctx.evalt.completely_evaluate<uint8_t>(content.d_data.low));
								os.put(lctx.evalt.completely_evaluate<uint8_t>(content.d_data.high));
								break;
							case parser::rawdata::WORD:
								{
									uint16_t x = lctx.evalt.completely_evaluate<uint16_t>(content.d_data.low);
									put(x);
									break;
								}
							case parser::rawdata::DOUBLEWORD:
								{
									uint32_t x = lctx.evalt.completely_evaluate<uint32_t>(content.d_data.low);
									put(x);
									break;
								}
							case parser::rawdata::QUADWORD:
								{
									uint64_t x = lctx.evalt.completely_evaluate<uint64_t>(content.d_data.low);
									put(x);
									break;
								}
						}
						break;
					case layt::concreteinsn::INSN:
						switch (content.i_subtype) {
							case layt::concreteinsn::I_SHORT:
								put(insn::build_short_insn(content.rd, content.ro, content.opcode));
								break;
							case layt::concreteinsn::I_TINY:
								put(insn::build_timm_insn(content.rd, lctx.evalt.completely_evaluate<uint32_t>(content.imm), content.opcode));
								break;
							// long insns
							case layt::concreteinsn::I_LONG:
								put(insn::build_imm_insn(content.rd, lctx.evalt.completely_evaluate<uint32_t>(content.imm), content.rs, content.ro, content.opcode));
								break;
							case layt::concreteinsn::I_BIG:
								put(insn::build_bigimm_insn(content.rd, lctx.evalt.completely_evaluate<uint32_t>(content.imm), content.opcode));
								break;
							case layt::concreteinsn::I_MED:
								put(insn::build_mediimm_insn(content.rd, lctx.evalt.completely_evaluate<uint32_t>(content.imm), content.ro, content.opcode));
								break;
							case layt::concreteinsn::I_MSM:
								put(insn::build_msmimm_insn(content.rd, lctx.evalt.completely_evaluate<uint32_t>(content.imm), content.FF, content.ro, content.opcode));
								break;
							case layt::concreteinsn::I_SM:
								put(insn::build_smimm_insn(content.rd, lctx.evalt.completely_evaluate<uint32_t>(content.imm), content.FF, content.rs, content.ro, content.opcode));
							default:
								break;
						}
					default:
						break;
				}
			}
			catch (std::domain_error &e) {
				::report_error(pctx, content.progpos, e.what());
			}
			current_address += content.length();
		}
	}
}
