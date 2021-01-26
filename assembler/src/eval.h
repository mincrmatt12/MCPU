#include <map>
#include <parser.h>
#include <stdexcept>

namespace masm::eval {
	struct evaluator {
		std::map<parser::labelname, parser::expr> labelvalues;

		// Write expr in a simpler way, making partial evaluates work better.
		void simplify(parser::expr& expr);

		// Evaluate an expression as far as possible, returning true if the result is reduced
		// to just a value (which also includes just a label)
		bool evaluate(parser::expr& expr);

		// Completely evaluate an expression, throwing if this is not possible. The return can be
		// either a labelname or any integer type.
		template<typename Result>
		Result completely_evaluate(parser::expr& expr) {
			if (!evaluate(expr)) {
				throw std::runtime_error("did not completely evaluate expression");
			}

			if constexpr (std::is_same_v<Result, parser::labelname>) {
				if (expr.type != parser::expr::label) throw std::runtime_error("invalid type for labelname");
				return expr.label_value;
			}
			else {
				static_assert(std::is_integral_v<Result>, "completely evaluate must give an integer");
				static_assert(sizeof(Result) <= sizeof(int64_t));

				if (expr.type != parser::expr::num) throw std::runtime_error("invalid type for num");

				// TODO: this needs to work properly on big endian machines
				
				Result val;
				memcpy(&val, &expr.constant_value, sizeof(Result));
				return val;
			}
		}

	private:
		// Implemented in c++ file since it's only referred to there.
		template<typename Func>
		void evaluate_commutative(parser::expr& expr, Func&& f);
		template<typename Func>
		void evaluate_noncommutative(parser::expr& expr, Func&& f);

		// Convert
		// add
		//  |- add
		//     |- num 2
		//     |- num 3
		//  |- num 1
		//
		// to
		//
		// add
		//  |- num 2
		//  |- num 3
		//  |- num 1
		bool simplify_flatten(parser::expr& expr);

		// Eliminate like terms TODO
		//
		// Allows for an offsetof-like macro to be simplified into a constant
		// for better instruction packing.
		bool simplify_eliminate(parser::expr& expr) {return false;}

		bool simplify_(parser::expr& expr);
	};
}
