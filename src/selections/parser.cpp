/* Chemfiles, an efficient IO library for chemistry file formats
 * Copyright (C) 2015 Guillaume Fraux
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/
 */
#include <algorithm>
#include <stack>
#include <map>

#include "chemfiles/Error.hpp"
#include "chemfiles/selections/expr.hpp"
#include "chemfiles/selections/parser.hpp"

using namespace chemfiles;
using namespace selections;

struct function_info_t {
    /// Function arity, i.e. number of arguments
    unsigned arity;
};

static std::map<std::string, function_info_t> FUNCTIONS = {
    {"name", {1}},
    {"mass", {1}},
    {"index", {1}},
    {"x", {1}},
    {"y", {1}},
    {"z", {1}},
    {"vx", {1}},
    {"vy", {1}},
    {"vz", {1}},
};

/// Is this token a function token?
static bool is_function(const Token& token) {
    return token.is_ident() && FUNCTIONS.find(token.ident()) != FUNCTIONS.end();
}

/* Standard shunting-yard algorithm, as described in Wikipedia
 * https://en.wikipedia.org/wiki/Shunting-yard_algorithm
 *
 * This convert infix expressions into an AST-like expression, while checking
 * parentheses.
 * The following input:
 *       name == bar and x <= 56
 * is converted to:
 *       and == bar name <= 56 x
 * which is the AST for
 *             and
 *         /          \
 *        ==          <=
 *       /  \        /  \
 *    name   bar    x    56
 */
static std::vector<Token> shunting_yard(token_iterator_t token, token_iterator_t end) {
    std::stack<Token> operators;
    std::vector<Token> output;
    while (token != end) {
        if (token->is_number() || token->is_variable()) {
            output.push_back(*token);
        } else if (token->is_ident()) {
            if (is_function(*token)) {
                operators.push(*token);
            } else {
                output.push_back(*token);
            }
        } else if (token->type() == Token::COMMA) {
            while (operators.top().type() != Token::LPAREN) {
                output.push_back(operators.top());
                operators.pop();
                if (operators.empty()) {
                    throw SelectionError(
                        "Mismatched paretheses or additional comma found"
                    );
                }
            }
        } else if (token->is_operator()) {
            while (!operators.empty()) {
                // All the operators are left-associative
                if (token->precedence() <= operators.top().precedence()) {
                    output.push_back(operators.top());
                    operators.pop();
                } else {
                    break;
                }
            }
            operators.push(*token);
        } else if (token->type() == Token::LPAREN) {
            operators.push(*token);
        } else if (token->type() == Token::RPAREN) {
            while (!operators.empty() &&
                   operators.top().type() != Token::LPAREN) {
                output.push_back(operators.top());
                operators.pop();
            }

            if (is_function(operators.top())) {
                output.push_back(operators.top());
                operators.pop();
            }

            if (operators.empty() || operators.top().type() != Token::LPAREN) {
                throw SelectionError("Mismatched parentheses");
            } else {
                operators.pop();
            }
        }
        token++;
    }
    while (!operators.empty()) {
        if (operators.top().type() == Token::LPAREN ||
            operators.top().type() == Token::RPAREN) {
            throw SelectionError("Mismatched parentheses");
        } else {
            output.push_back(operators.top());
            operators.pop();
        }
    }
    // AST come out as reverse polish notation, let's reverse it for easier
    // parsing after
    std::reverse(std::begin(output), std::end(output));
    return output;
}

static bool have_short_form(const std::string& expr) {
    return expr == "name" || expr == "index" || expr == "mass";
}

/* Rewrite the token stream to convert short form for the expressions to the
 * long one.
 *
 * Short forms are expressions like `name foo` or `index 3`, which are
 * equivalent
 * to `name == foo` and `index == 3`.
 */
static std::vector<Token> clean_token_stream(std::vector<Token> stream) {
    auto out = std::vector<Token>();
    for (auto it = stream.cbegin(); it != stream.cend(); it++) {
        if (it->is_ident() && have_short_form(it->ident())) {
            auto next = it + 1;
            if (next != stream.cend() && !next->is_operator()) {
                out.emplace_back(*it);
                out.emplace_back(Token(Token::EQ));
                continue;
            }
        }
        out.emplace_back(*it);
    }
    return out;
}

Ast selections::dispatch_parsing(token_iterator_t& begin, const token_iterator_t& end) {
    if (begin->is_boolean_op()) {
        switch (begin->type()) {
        case Token::AND:
            return parse<AndExpr>(begin, end);
        case Token::OR:
            return parse<OrExpr>(begin, end);
        case Token::NOT:
            return parse<NotExpr>(begin, end);
        default:
            throw SelectionError("Unknown boolean operator. This is a bug.");
        }
    } else if (begin->is_binary_op()) {
        if ((end - begin) < 3 || begin[2].type() != Token::IDENT) {
            throw SelectionError("Bad binary operation around " + begin->str());
        }

        auto ident = begin[2].ident();
        if (ident == "name") {
            return parse<NameExpr>(begin, end);
        } else if (ident == "index") {
            return parse<IndexExpr>(begin, end);
        } else if (ident == "mass") {
            return parse<MassExpr>(begin, end);
        } else if (ident == "x" || ident == "y" || ident == "z") {
            return parse<PositionExpr>(begin, end);
        } else if (ident == "vx" || ident == "vy" || ident == "vz") {
            return parse<VelocityExpr>(begin, end);
        } else {
            throw SelectionError("Unknown operation: " + ident);
        }
    } else if (begin->is_ident()) {
        auto ident = begin->ident();
        if (ident == "all") {
            return parse<AllExpr>(begin, end);
        } else if (ident == "none") {
            return parse<NoneExpr>(begin, end);
        } else {
            throw SelectionError("Unknown operation: " + ident);
        }
    } else {
        throw SelectionError("Could not parse the selection");
    }
}

Ast selections::parse(std::vector<Token> token_stream) {
    token_stream = clean_token_stream(token_stream);
    auto rpn = shunting_yard(std::begin(token_stream), std::end(token_stream));

    auto begin = rpn.cbegin();
    const auto end = rpn.cend();
    auto ast = dispatch_parsing(begin, end);

    if (begin != end)
        throw SelectionError("Could not parse the end of the selection.");
    return ast;
}