#include "eval.h"
#include <algorithm>
#include "dbg.h"

namespace masm::eval {
	template<typename Func>
	void evaluator::evaluate_commutative(parser::expr& expr, Func&& func) {
		auto rng = expr.args | std::views::filter([](const parser::expr& e){return e.type == parser::expr::num;});
		// Allocate a scratch instance to add nonfinished bits
		std::list<parser::expr> new_e{/* get first number value */
			parser::expr(
				std::ranges::begin(rng)->constant_value
			)
		};

		// Accumulate remaining constants
		for (const auto& i : expr.args | std::views::filter([](const parser::expr& e){return e.type == parser::expr::num;}) | std::views::drop(1))
			new_e.front().constant_value = func(new_e.front().constant_value, i.constant_value);

		// Splice remaining components

		for (auto& i : expr.args | std::views::filter([](const parser::expr& e){return e.type != parser::expr::num;})) {
			new_e.emplace_back(std::move(i));
		}

		// Swap out expression
		std::swap(expr.args, new_e);

		// If resulting instance has only one argument, replace it
		if (expr.args.size() == 1) expr = expr.args.front();
	}

	template<typename Func>
	void evaluator::evaluate_noncommutative(parser::expr& expr, Func&& func) {
		std::list<parser::expr> new_e{std::move(expr.args.front())};

		for (auto& i : expr.args | std::views::drop(1)) {
			// If this is not a number, add to the new_e
			if (i.type != parser::expr::num)
				add:
					new_e.emplace_back(std::move(i));
			else {
				// If the last entry is a number, calculate it
				if (new_e.back().type == parser::expr::num)
					new_e.back().constant_value = func(new_e.back().constant_value, i.constant_value);
				else goto add;
			}
		}

		// Swap out expression
		std::swap(expr.args, new_e);

		// If resulting instance has only one argument, replace it
		if (expr.args.size() == 1) expr = expr.args.front();
	}

	bool evaluator::evaluate(parser::expr& expr) {
		// Evaluate all subexpressions so that we only have to deal with nums / labels
		bool is_finished = true;
		for (auto& subexpr : expr.args) is_finished = evaluate(subexpr) && is_finished;

		// If this is a number, return now
		if (expr.type == parser::expr::num) return true;

		// If this is a label and we know what the value is, sub it in.
		if (expr.type == parser::expr::label) {
			if (labelvalues.contains(expr.label_value)) {
				expr = parser::expr{labelvalues[expr.label_value]}; // explicitly copy
				return evaluate(expr);
			}
			// otherwise just return false
			return false;
		}

		// If no numerical components, return early
		if (!std::ranges::any_of(expr.args, [](const auto& e){return e.type == parser::expr::num;})) return false;

		// Otherwise, evaluate the expression.
		switch (expr.type) {
			// One operand only
			case parser::expr::neg:
				if (is_finished) {
					// negate value
					expr.replace(-expr.args.front().constant_value);
					return true;
				}
				return false;

			// Two operand only
			case parser::expr::lshift:
			case parser::expr::rshift:
				if (is_finished) {
					if (expr.type == parser::expr::lshift)
						expr.replace(expr.args.front().constant_value << expr.args.back().constant_value);
					else
						expr.replace(expr.args.front().constant_value >> expr.args.back().constant_value);
					return true;
				}
				return false;

			// Commutative
			case parser::expr::add:
				evaluate_commutative(expr, std::plus{});
				break;
			case parser::expr::mul:
				evaluate_commutative(expr, std::multiplies{});
				break;
				
			case parser::expr::div:
				evaluate_commutative(expr, std::divides{});
				break;
			case parser::expr::mod:
				evaluate_commutative(expr, std::modulus{});
				break;
			default:
				throw std::logic_error("invalid type in evaluate");
		}

		return is_finished;
	}

	bool evaluator::simplify_(parser::expr& e) {
		evaluate(e);
		return simplify_eliminate(e) || simplify_flatten(e) || std::ranges::any_of(e.args, [&](auto& e){return simplify_(e);});
	};

	void evaluator::simplify(parser::expr& e) {
		while (
			simplify_(e)
		) {;}
	}

	bool evaluator::simplify_flatten(parser::expr& e) {
		switch (e.type) {
			case parser::expr::mul:
			case parser::expr::add:
			case parser::expr::div:
			case parser::expr::mod:
				break;
			default:
				return false;
		}

		bool v = false;

		std::list<parser::expr> new_args{};
		for (auto&d : e.args) {
			if (d.type != e.type) new_args.emplace_back(std::move(d));
			else {
				v = true;
				new_args.splice(new_args.end(), d.args);
			}
		}

		std::swap(e.args, new_args);
		if (e.args.size() == 1) e = e.args.front();
		return v;
	}
}
