%skeleton "lalr1.cc"
%define api.parser.class {mcasm_parser}
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
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <iomanip>

#define ENUM_SIMPLE_EXPRESSIONS(o) \
	o(add) o(sub) o(mul) o(div) o(mod) o(lshift) o(rshift) o(neg)

#define ENUM_EXPRESSIONS(o) \
	ENUM_SIMPLE_EXPRESSIONS(o) \
	o(num) o(label) o(undef)

namespace masm::parser {
	struct loadstore_insn {
		masm::insn::load_store_kind::e kind{};
		masm::insn::load_store_size::e size{};
		masm::insn::load_store_dest::e dest{};

		loadstore_insn(
			masm::insn::load_store_kind::e kind,
			char dest,
			masm::insn::load_store_size::e size=masm::insn::load_store_size::HALFWORD
		) : kind(kind), size(size) {
			switch (dest) {
				case 's': this->dest = masm::insn::load_store_dest::SEXT; break;
				case 'l': this->dest = masm::insn::load_store_dest::LOWW; break;
				case 'h': this->dest = masm::insn::load_store_dest::HIGHW; break;
				default:  this->dest = masm::insn::load_store_dest::ZEXT; break;
			}
		}
		
		loadstore_insn() = default;
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
		bool is_jmp = false;
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
				this->condition = UNDEFINED;
			}
#undef o
		}

		mov_insn() = default;

		bool needs_swap(c &to) const {
#define o(nv, _, ns, nt) if (condition == nv && ns) {to = nt; return true;} else
			ENUM_MOV_CONDS(o) return false;
#undef o
		}

		const char * condition_string() const {
#define o(nv, nn, _, __) if (condition == nv) {return nn;} else
			ENUM_MOV_CONDS(o) return "undef";
#undef o
		}
	};

#undef ENUM_MOV_CONDS

	struct labelname {
		size_t section = 0;
		size_t index = 0;
	};

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

		explicit expr(int64_t constant) : type(num), constant_value(constant) {}
		explicit expr(const labelname &lbl) : type(label), label_value(lbl) {}

		expr() = default;
	};

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
		expr constant = expr(0);
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
		masm::insn::alu_op::e i_alu;
		
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
		insn(const masm::insn::alu_op::e &opcode, T&& ...args) :
			type(ALU), i_alu(opcode), args{std::forward<T>(args)...} {}

		insn(const labelname& lbl) :
			type(LABEL), lbl(lbl) {}
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

	struct pctx;
}

}

%param {masm::parser::pctx &ctx}

%code provides {
namespace masm::parser {

struct pctx {
	const char *cursor, *start;
	yy::location loc;
	std::vector<ptrdiff_t> lineoffsets;
	
	std::vector<section> sections;

	std::unordered_map<std::string, labelname> local_labels;
	std::unordered_set<size_t> defined_local_labels;
	std::unordered_map<std::string, labelname> global_labels;

	std::vector<insn_arg> address_components;

	void prepare_cursor(const char *newcursor) {
		ptrdiff_t o = 0;
		const char *lt = newcursor;

		lineoffsets.clear();
		lineoffsets.push_back(0);
		while (*lt) {
			if (*(lt - 1) == '\n') {
				lineoffsets.push_back(o);
			}
			++o;
			++lt;
		}

		cursor = start = newcursor;
	}

	labelname define_label(std::string name, bool by_use=false) {
		if (sections.empty()) throw yy::mcasm_parser::syntax_error(loc, "defined label before section started");
		// if there's a label with this name defined locally, but it has never been previously set, return it
		if (local_labels.count(name) && !defined_local_labels.count(local_labels[name].index)) {
			labelname prev_lbl = local_labels[name];
			defined_local_labels.insert(prev_lbl.index);
			sections.back().instructions.emplace_back(prev_lbl); // add the label into the insns
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

	void begin_address() {
		// If already declaring, raise error
		if (!address_components.empty()) throw yy::mcasm_parser::syntax_error(loc, "nested addresses");
	}

	address end_address() {
		address result;
		bool constant_added = false;

		auto t_address_components = this->address_components;
		this->address_components.clear();

		for (auto& comp : t_address_components) {
			// If this is an expression, add it to the constant
			if (comp.mode == insn_arg::CONSTANT) {
				if (!constant_added) {
					constant_added = true;
					result.constant = std::move(comp.constant);
				}
				else {
					// Otherwise, add the value with an e_add
					result.constant = expr::make_add(std::move(result.constant), std::move(comp.constant));
				}
			}
			// If this is a register, set it to the base
			else if (comp.mode == insn_arg::REGISTER) {
				if (result.reg_base != 0) {
					// Can we instead use the index register?
					if (result.reg_index == 0) {
						result.reg_index = comp.reg;
					}
					else throw yy::mcasm_parser::syntax_error(loc, "too many register parts for address");
				}
				else {
					result.reg_base = comp.reg;
				}
			}
			else if (comp.mode == insn_arg::REGISTER_LSHIFT) {
				// If index reg is used, throw
				if (result.reg_index != 0) {
					throw yy::mcasm_parser::syntax_error(loc, "too many index registers for address");
				}
				result.reg_index = comp.reg;
				result.shift = comp.shift;
			}
			else throw yy::mcasm_parser::syntax_error(loc, "invalid address component");
		}

		return result;
	}

	void define_address_component(bool add, insn_arg&& comp) {
		if (comp.mode == insn_arg::CONSTANT) {
			if (!add) {
				comp.constant = expr::make_neg(std::move(comp.constant));
			}
			address_components.emplace_back(std::move(comp));
		}
		else {
			if (!add) throw yy::mcasm_parser::syntax_error(loc, "cannot subtract registers");
			address_components.emplace_back(std::move(comp));
		}
	}

};

}
}

%token END 0
%token LSHIFT "<<" RSHIFT ">>"
%token ID_ORG ".org"
%token LOADSTORE_INSN "load/store instruction" ALU_INSN "alu instruction" MOV_INSN "mov instruction" JMP_INSN "jmp instruction"
%token IDENTIFIER "name" REGISTER "register" NUMBER "number"

%type<int64_t> NUMBER
%type<uint32_t> REGISTER
%type<std::string> IDENTIFIER label
%type<masm::parser::loadstore_insn> LOADSTORE_INSN
%type<masm::insn::alu_op::e> ALU_INSN
%type<masm::parser::mov_insn> MOV_INSN JMP_INSN
%type<masm::parser::insn> instruction
%type<masm::parser::insn_arg> aluop2 movop addresscomponent movtarget
%type<masm::parser::expr> expr mexpr
%type<masm::parser::address> address

%left "<<" ">>"
%left '+' '-'
%left '*' '/' '%'
%right NEG
%left '('

%code
{

namespace yy {mcasm_parser::symbol_type yylex(masm::parser::pctx &ctx); }

#define MN   masm::parser
#define M(x) std::move(x)
#define C(x) std::decay_t<decltype(x)>(x)

}

%%

object: defs {ctx.end_section();};

defs: defs directive
	| defs label         { ctx.define_label($2); }
	| defs instruction   { ctx.add_insn(M($2)); }
	| defs error
	| %empty
	;

label: IDENTIFIER ':' { $$ = $1; }

instruction: LOADSTORE_INSN REGISTER ',' address                 { $$ = MN::insn($1, $4, $2); }
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

address: '[' {ctx.begin_address();} addresscomponents ']' { $$ = ctx.end_address(); }
	   | '[' error ']'                                    { $$ = MN::address{}; }
	   ;

addresscomponents: addresscomponent                           { ctx.define_address_component(true, M($1)); }
				 | addresscomponents '+' addresscomponent     { ctx.define_address_component(true, M($3)); }
				 | addresscomponents '-' addresscomponent     { ctx.define_address_component(false, M($3)); }
				 ;

addresscomponent: expr                  { $$ = MN::insn_arg(M($1)); }
				| REGISTER              { $$ = MN::insn_arg($1); }
				| REGISTER "<<" NUMBER  { $$ = MN::insn_arg($1, MN::insn_arg::REGISTER_LSHIFT, $3); }
				;

directive: ".org" expr { ctx.start_section(M($2)); }
		 ;

expr: NUMBER        { $$ = MN::expr($1); }
	| IDENTIFIER    { $$ = MN::expr(ctx.lookup($1)); }
	| '(' mexpr ')' { $$ = M($2); }
	| '(' error ')' { $$ = MN::expr{}; }
	;

mexpr: NUMBER                             { $$ = MN::expr($1); }
	 | IDENTIFIER                         { $$ = MN::expr(ctx.lookup($1)); }
	 | '(' mexpr ')'                      { $$ = M($2); }
	 | '(' error ')'                      { $$ = MN::expr{}; }
	 | mexpr '+' mexpr                    { $$ = MN::expr::make_add(M($1), M($3)); }
	 | mexpr '-' mexpr                    { $$ = MN::expr::make_sub(M($1), M($3)); }
	 | mexpr '*' mexpr                    { $$ = MN::expr::make_mul(M($1), M($3)); }
	 | mexpr '/' mexpr                    { $$ = MN::expr::make_div(M($1), M($3)); }
	 | mexpr '%' mexpr                    { $$ = MN::expr::make_mod(M($1), M($3)); }
	 | mexpr "<<" mexpr                   { $$ = MN::expr::make_lshift(M($1), M($3)); }
	 | mexpr ">>" mexpr                   { $$ = MN::expr::make_rshift(M($1), M($3)); }
	 | '-' mexpr          %prec NEG       { $$ = MN::expr::make_neg(M($2)); }
	 ;
	  
%%

yy::mcasm_parser::symbol_type yy::yylex(masm::parser::pctx& ctx) {
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

"ld" ("." [slh])?   { return tk(LOADSTORE_INSN, masm::parser::loadstore_insn(masm::insn::load_store_kind::LOAD, (ctx.cursor - anchor > 2 ? anchor[3] : 'z'))); }
"ld.b" ("." [slh])? { return tk(LOADSTORE_INSN, masm::parser::loadstore_insn(masm::insn::load_store_kind::LOAD, (ctx.cursor - anchor > 4 ? anchor[5] : 'z'), masm::insn::load_store_size::BYTE)); }
"st." [lh]          { return tk(LOADSTORE_INSN, masm::parser::loadstore_insn(masm::insn::load_store_kind::STORE, anchor[3])); }
"st.b." [lh]        { return tk(LOADSTORE_INSN, masm::parser::loadstore_insn(masm::insn::load_store_kind::STORE, anchor[5], masm::insn::load_store_size::BYTE)); }

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

("mov" | "jmp") ("." ("s"?("lt"|"gt"|"le"|"ge")|("eq"|"ne"|"bs")))?  { 
	if (*anchor == 'j')
		return tk(JMP_INSN, masm::parser::mov_insn(true, (ctx.cursor - anchor > 3 ? std::string(anchor + 4, ctx.cursor) : std::string{})));
	else
		return tk(MOV_INSN, masm::parser::mov_insn(false, (ctx.cursor - anchor > 3 ? std::string(anchor + 4, ctx.cursor) : std::string{})));
}

// Registers

"r" [0-9][0-9]?         { uint32_t v; std::from_chars(anchor + 1, ctx.cursor, v); return tk(REGISTER, v); }

// Identifiers

[a-zA-Z_] [a-zA-Z_0-9]* { return tk(IDENTIFIER, std::string(anchor, ctx.cursor)); }

// Numbers

[0-9]+         { int64_t v; std::from_chars(anchor, ctx.cursor, v); return tk(NUMBER, v); }
"0x" [0-9a-fA-F]+ { int64_t v; std::from_chars(anchor + 2, ctx.cursor, v, 16); return tk(NUMBER, v); }
"0b" [10]+        { int64_t v; std::from_chars(anchor + 2, ctx.cursor, v, 2); return tk(NUMBER, v); }

// Whitespace and ignored things
"\000"          { return tk(END); }
"\r\n" | [\r\n] { ctx.loc.lines();	return yylex(ctx); }
"//" [^\r\n]*   { return yylex(ctx); }
[\t\v\b\f ]     { ctx.loc.columns();	return yylex(ctx); }

// Operators

"<<"            { return tk(LSHIFT); }
">>"            { return tk(RSHIFT); }

// Invalid

.               { return s([](auto... s) { return mcasm_parser::symbol_type(s...);}, mcasm_parser::token_type(ctx.cursor[-1]&0xFF)); }

%}
	#undef tk
}

void yy::mcasm_parser::error(const location_type &l, const std::string &m) {
	std::cerr << (l.begin.filename ? l.begin.filename->c_str() : "(undefined)");
	std::cerr << ':' << l.begin.line << ':' << l.begin.column << '-' << l.end.column << ": " << m << '\n';

	try {
		const char * line = ctx.start + ctx.lineoffsets.at(l.begin.line - 1);
		std::cerr << std::setw(6) << std::right << l.begin.line << " | ";
		while (*line != '\n') {
			std::cerr << *line++;
		}
		std::cerr << "\n         ";
		for (size_t i = 0; i < l.begin.column - 1; ++i) {
			std::cerr << ' ';
		}
		for (size_t i = 0; i < (l.end.column - l.begin.column); ++i) {
			std::cerr << (i == 0 ? '^' : '~');
		}
		std::cerr << "\n";
	}
	catch (const std::out_of_range &e) {}
}
