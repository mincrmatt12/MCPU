%skeleton "lalr1.cc"
%define parser_class_name {mcasm_parser}
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%define parse.error verbose
%require "3.2"
%locations

%code requires {

#include <stdint.h>
#include <string>
#include <charconv>
#include <insns.h>

#define ENUM_SIMPLE_EXPRESSIONS(o) \
	o(add) o(sub) o(mul) o(div) o(mod) o(lshift) o(rshift) o(neg)

#define ENUM_EXPRESSIONS(o) \
	ENUM_SIMPLE_EXPRESSIONS(o) \
	o(num) o(label) o(undef)

namespace masm::parser {
	struct loadstore_insn {
		using namespace masm::insn;

		load_store_kind::e kind;
		load_store_size::e size;
		load_store_dest::e dest;

		loadstore_insn(
			load_store_kind::e kind,
			char dest,
			load_store_size::e size=load_store_size::HALFWORD
		) : kind(kind), size(size), {
			switch (dest) {
				case 's': dest = load_store_dest::SEXT; break;
				case 'l': dest = load_store_dest::LOWW; break;
				case 'h': dest = load_store_dest::HIGHW; break;
				default:  dest = load_store_dest::ZEXT; break;
			}
		}
	};

#define ENUM_MOV_CONDS(o) \
	o(AL, "", true, AL) \
	o(LT, "lt", true, LT) \
	o(SLT, "slt", true, SLT) \
	o(GT, "gt", false, LT) \
	o(SGT, "sgt", false, SLT) \
	o(LE, "le", false, GE) \
	o(SLE, "sle", false, SGE) \
	o(GE, "ge", true, GE) \
	o(SGE, "sge", true, SGE) \
	o(BS, "bs", true, BS)

	struct mov_insn {
		bool is_jmp;
		enum c {
#define o(n, _, __, ___) n,
			ENUM_MOV_CONDS(o)
#undef o
			UNDEFINED = -1
		} condition = UNDEFINED;

		mov_insn(
			bool is_jmp,
			const std::string &condition
		) : is_jmp(is_jmp) {
#define o(nv, nn, _, __) if (condition == nn) this->condition = nv; else
			ENUM_MOV_CONDS(o) {
				// else
				condition = UNDEFINED;
			}
#undef o
		}

		bool needs_swap(c &to) const {
#define o(nv, _, ns, nt) if (condition == nv && ns) {to = nt; return true}; else
			ENUM_MOV_CONDS(o) return false;
#undef o
		}
	};

#undef ENUM_MOV_CONDS

	struct labelname {
		size_t section = 0;
		size_t index = 0;
	}

	struct expr {
		enum t {
#define o(n) n,
			ENUM_EXPRESSIONS(o)
#undef o
		} type = undef;

		int64_t constant_value;
		labelname label_value;

		std::vector<expr> args;

#define o(n) \
		template<typename ...T> \
		inline static expr make_##n(T&& ...args) { \
			return expr(expr::n, std::forward<T>(args)...); \
		}
		
		ENUM_SIMPLE_EXPRESSIONS(o)
#undef o

		template<typename ...T>
		expr(t type, T&& ...args) : type(type), args{std::forward<T>(args)...} {}

		expr(int64_t constant) explicit : type(num), constant_value(constant) {}
		expr(const labelname &lbl) explicit : type(label), label_value(lbl) {}
	}

	struct insn_arg {
		enum m {
			REGISTER,
			CONSTANT,
			REGISTER_LSHIFT,
			REGISTER_RSHIFT,
			REGISTER_PLUS,
			UNDEFINED = -1
		} mode = UNDEFINED;

		uint32_t reg = 0; // r0 is a useful default in most cases
		expr constant;
		uint8_t shift = 0;

		insn_arg() = default;
		insn_arg(expr &&constant) : mode(CONSTANT), constant(std::move(constant)) {}
		insn_arg(uint32_t reg) : mode(REGISTER), reg(reg) {}
		insn_arg(uint32_t reg, expr &&constant) : mode(REGISTER_PLUS), constant(std::move(constant)) {}
		insn_arg(uint32_t reg, m mode, uint8_t shift) : mode(mode), reg(reg), shift(shift) {}
	};

	struct address {
		uint32_t reg_base = 0, reg_index = 0;
		uint8_t shift = 0;
		expr constant;
	};

	struct insn {
		enum {
			LOADSTORE,
			MOV,
			ALU,
			LABEL,
			UNDEFINED = -1
		} type = UNDEFINED;

		loadstore_insn i_ls;
		mov_insn i_mov;
		masm::insn::alu_op i_alu;
		
		address addr;
		labelname lbl;
		std::vector<insn_arg> args;

		insn() = default;
		
		template<typename ...T>
		insn(const loadstore_insn &opcode, const address& addr, T&& ...args) :
			type(LOADSTORE), i_ls(opcode), addr(addr), args{std::forward<T>(args)...} {}
		
		template<typename ...T>
		insn(const mov_insn &opcode, T&& ...args) :
			type(MOV), i_mov(opcode), args{std::forward<T>(args)...} {}
		
		template<typename ...T>
		insn(const masm::insn::alu_op &opcode, T&& ...args) :
			type(ALU), i_ls(opcode), args{std::forward<T>(args)...} {}

		insn(const labelname& lbl) :
			type(LABEL), labelname(lbl) {}
	};

	struct section {
		expr starting_address; // 0xffff'ffff for position independent
		size_t index = 0;
		std::vector<insn> instructions;
		size_t num_labels = 0;

		labelname new_label() {
			labelname lbl;
			lbl.section = index;
			lbl.index = num_labels++;
			return lbl;
		}
	};
}

}

%param {masm::parser::pctx &ctx}
%code provides {
namespace masm::parser {

struct pctx {
	const char *cursor;
	yy::location loc;
	
	std::vector<section> sections;
	std::unordered_map<std::string, labelname> local_labels;
	std::unordered_set<size_t> defined_local_labels;
	std::unordered_map<std::string, labelname> global_labels;

	labelname define_label(std::string name, bool by_use=false) {
		if (sections.empty()) throw yy::mcasm_parser::syntax_error(loc, "defined label before section started");
		// if there's a label with this name defined locally, but it has never been previously set, return it
		if (local_labels.count(name) && !defined_local_labels.count(local_labels[name].index)) {
			labelname prev_lbl = local_labels[name];
			defined_local_labels.insert(prev_lbl.index);
			return prev_lbl;
		}
		labelname lbl = sections.back().new_label();
		local_labels[name] = lbl; // this intentionally overwrites prior entries to allow for repeated labels for things like loops
		if (!by_use) {
			defined_local_labels.insert(lbl.index); // mark this as used
			sections.back().instructions.emplace_back(lbl); // add the label
		}
		return lbl;
	}

	labelname lookup(std::string name) {
		// search for local label first, local labels override global ones
		if (local_labels.count(name)) {
			return local_labels[name];
		}
		// then search for a global label
		if (global_labels.count(name)) {
			return global_labels[name];
		}
		// otherwise, define a new label 
		return define_label(name, true);
	}

	void globalize(std::string name) {
		if (global_labels.count(name)) {
			throw yy::mcasm_parser::syntax_error(loc, "multiple conflicting definitions for global label " + name);
		}
		labelname target = lookup(name); // this also handles creating the label if not already defined
		// add target to global table
		global_labels[name] = target;
	}

	void end_section() {
		// Make sure all labels were defined
		if (sections.back().num_labels != defined_local_labels.size()) {
			// Find the first undefined label
			for (const auto& [name, lbl] : local_labels) {
				if (!defined_local_labels.count(lbl.index)) throw yy::mcasm_parser::syntax_error(loc, "local label " + name + " was never defined");
			}
		}
		// Clear local label data for next section
		local_labels.clear();
		defined_local_labels.clear();
	}

	void start_section(expr &&starting_address) {
		if (!sections.empty()) end_section();
		section new_section;
		new_section.starting_address = std::move(starting_address);
		new_section.index = sections.size();
		sections.emplace_back(std::move(new_section));
	}
	
	void add_insn(insn &&i) {
		if (sections.empty()) throw yy::mcasm_parser::syntax_error(loc, "instructions before section start");
		sections.back().instructions.emplace_back(std::move(i));
	}
};

}
}

%token END 0
%token LSHIFT "<<" RSHIFT ">>"
%token ID_ORG ".org"
%token LOADSTORE_INSN ALU_INSN MOV_INSN JMP_INSN
%token IDENTIFIER REGISTER
%token DEC_NUMBER HEX_NUMBER BIN_NUMBER

%type<int64_t> NUMBER
%type<uint32_t> REGISTER
%type<std::string> IDENTIFIER label
%type<masm::parser::loadstore_insn> LOADSTORE_INSN
%type<masm::insn::alu_op> ALU_INSN
%type<masm::parser::mov_insn> MOV_INSN JMP_INSN
%type<masm::parser::insn> instruction
%type<masm::parser::insn_arg> aluop2 movop addresscomponent movtarget
%type<masm::parser::expr> expr


%code
{

namespace yy {mlang_parser::symbol_type yylex(parsecontext &ctx); }

#define MN   masm::parser
#define M(x) std::move(x)
#define C(x) std::decay_t<decltype(x)>(x)

}

%%

object: defs;

defs: defs directive
	| defs label         { ctx.define_label($2); }
	| defs instruction   { ctx.add_insn(M($2)); }
	| %empty
	;

label: IDENTIFIER ':' { $$ = $1; }

instruction: LOADSTORE_INSN REGISTER ',' address                 { $$ = MN::insn($1, $3, $2); }
		   | ALU_INSN REGISTER ',' REGISTER ',' aluop2           { $$ = MN::insn($1, $2, $4, $6); }
		   | MOV_INSN REGISTER ',' movtarget                     { $$ = MN::insn($1, $2, $4); }
		   | MOV_INSN REGISTER ',' movtarget ',' movop ',' movop { $$ = MN::insn($1, $2, $4, $6, $8); }
		   | JMP_INSN movtarget                                  { $$ = MN::insn($1, $2); }
		   | JMP_INSN movtarget ',' movop ',' movop              { $$ = MN::insn($1, $2, $4, $6); }
		   ;
	
aluop2: expr                    { $$ = MN::insn_arg(M($1)); }
	  | REGISTER                { $$ = MN::insn_arg($1); }
	  | REGISTER "<<" NUMBER    { $$ = MN::insn_arg($1, MN::insn_arg::REGISTER_LSHIFT, $3); }
	  | REGISTER ">>" NUMBER    { $$ = MN::insn_arg($1, MN::insn_arg::REGISTER_RSHIFT, $3); }
	  ;

movtarget: expr               { $$ = MN::insn_arg(M($1)); }
		 | REGISTER           { $$ = MN::insn_arg($1); }
		 | REGISTER '+' expr  { $$ = MN::insn_arg($1, M($3)); }
		 | REGISTER '-' expr  { $$ = MN::insn_arg($1, MN::expr::make_neg(M($3))); }
		 ;

movop: expr     { $$ = MN::insn_arg(M($1)); }
	 | REGISTER { $$ = MN::insn_arg($1); }
	 ;

address: '[' addresscomponents ']';

addresscomponents: addresscomponent
				 | addresscomponents '+' addresscomponent
				 | addresscomponents '-' addresscomponent
				 ;

addresscomponent: expr                  { $$ = MN::insn_arg(M($1)); }
				| REGISTER              { $$ = MN::insn_arg($1); }
				| REGISTER "<<" NUMBER  { $$ = MN::insn_arg($1, MN::insn_arg::REGISTER_LSHIFT, $3); }
				;

directive: ".org" expr { ctx.start_section(M($2)); }
		 ;

expr: NUMBER
	| IDENTIFIER
	;
	  
%%

yy::mlang_parser::symbol_type yy::yylex(parsecontext& ctx) {
	const char* anchor = ctx.cursor;
	ctx.loc.step();

	// Helper lookup tables

	const char * re2c_marker;
	auto s = [&](auto func, auto&&... params){ctx.loc.columns(ctx.cursor - anchor); return func(params..., ctx.loc);};
	#define tk(t, ...) s(yy::mcasm_parser::make_##t, ##__VA_ARGS__)
%{
// re2c lexer here:
// note that spaces are ignored in the regex thing
re2c:yyfill:enable	= 0;
re2c:define:YYCTYPE	= "char";
re2c:define:YYCURSOR	= "ctx.cursor";
re2c:define:YYMARKER	= "re2c_marker";

// Directives

".org"              { return tk(ID_ORG); }

// Instructions

/// LOAD/STORE

"ld" ("." [slh])?   { return tk(LOADSTORE_INSN, masm::insn::load_store_kind::LOAD, (ctx.cursor - anchor == 4 ? anchor[3] : 'z')); }
"ld.b" ("." [slh])? { return tk(LOADSTORE_INSN, masm::insn::load_store_kind::LOAD, masm::insn::load_store_size::BYTE, (ctx.cursor - anchor == 6 ? anchor[5] : 'z')); }
"st." [lh]          { return tk(LOADSTORE_INSN, masm::insn::load_store_kind::STORE, anchor[3]); }
"st.b." [lh]        { return tk(LOADSTORE_INSN, masm::insn::load_store_kind::STORE, masm::insn::load_store_size::BYTE, anchor[5]); }

/// ALU

"add"               { return tk(ALU_INSN, masm::insn::alu_op::ADD); }
"sub"               { return tk(ALU_INSN, masm::insn::alu_op::SUB); }
"sl"                { return tk(ALU_INSN, masm::insn::alu_op::SL); }
"sr"                { return tk(ALU_INSN, masm::insn::alu_op::SR); }
"lsl"               { return tk(ALU_INSN, masm::insn::alu_op::LSL); }
"lsr"               { return tk(ALU_INSN, masm::insn::alu_op::LSR); }
"or"                { return tk(ALU_INSN, masm::insn::alu_op::OR); }
"eor"               { return tk(ALU_INSN, masm::insn::alu_op::EOR); }
"and"               { return tk(ALU_INSN, masm::insn::alu_op::AND); }
"nor"               { return tk(ALU_INSN, masm::insn::alu_op::NOR); }
"enor"              { return tk(ALU_INSN, masm::insn::alu_op::ENOR); }
"nand"              { return tk(ALU_INSN, masm::insn::alu_op::NAND); }

/// MOVS

("mov" | "jmp") ("." [a-z][a-z][a-z]?)?  { 
	if (*anchor == 'j')
		return tk(JMP_INSN, true, (ctx.cursor - anchor > 3 ? std::string(anchor + 4, ctx.cursor) : ""));
	else
		return tk(MOV_INSN, false, (ctx.cursor - anchor > 3 ? std::string(anchor + 4, ctx.cursor) : ""));
}

// Identifiers

[a-zA-Z_] [a-zA-Z_0-9]* { return tk(IDENTIFIER, std::string(anchor, ctx.cursor)); }

// Registers

r\d\d?             { uint32_t v; std::from_chars(anchor + 1, ctx.cursor, v); return tk(REGISTER, v) }

// Numbers

[-]?[0-9]+         { int64_t v; std::from_chars(anchor, ctx.cursor, v); return tk(NUMBER, v); }
[-]?0x[0-9a-fA-F]+ { int64_t v;
					 std::from_chars(*anchor == '-' ? anchor + 3 : anchor + 2, ctx.cursor, v, 16); 
					 return tk(NUMBER, *anchor == '-' ? -v : v);
				   }
0b[10]+            { int64_t v; std::from_chars(anchor + 2, ctx.cursor, v); return tk(NUMBER, v); }

// Whitespace and ignored things
"\000"          { return tk(END); }
"\r\n" | [\r\n] { ctx.loc.lines();	return yylex(ctx); }
"//" [^\r\n]*   { return yylex(ctx); }
[\t\v\b\f ]     { ctx.loc.columns();	return yylex(ctx); }

// Operators

"<<"            { return tk(LSHIFT); }
">>"            { return tk(RSHIFT); }

// Invalid

.               { return s([](auto... s) { return mlang_parser::symbol_type(s...);}, mlang_parser::token_type(ctx.cursor[-1]&0xFF)); }

%}
	#undef tk
}
