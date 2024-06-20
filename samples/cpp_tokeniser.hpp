#pragma once

#include "tokeniser.hpp"

namespace cpp_tokeniser {

class cpp_tokeniser_state_machine
  : public tokeniser::tokeniser_state_machine_generic<cpp_tokeniser_state_machine>
{
  public:
    enum class operator_type
    {
        assignment, asterisk,
        binary_not, binary_not_eq, binary_or,
        close_curly_bracket, close_paren, close_sq_bracket, colon, comma,
        decrement, divide, divide_equal,
        equal,
        increment,
        logical_and, logical_or,
        minus, minus_equal, multiply_equal, // '*' is ambiguous, dependent on context
        op_gt, op_gte, op_lt, op_lte, open_curly_bracket, open_paren, open_sq_bracket,
        period, plus, plus_equal, power,
        scope_resolution, semicolon, shift_left, shift_right,
    };

    using operator_info_type = std::pair<operator_type, std::string_view>;

#ifdef _DEBUG
    cpp_tokeniser_state_machine()
    {
        for (auto op : operators_) {
            for (auto ch : op.first)
                assert(is_operator_char(ch));
        }
    }
#endif

    bool const greedy_operator_check(std::string_view op) const noexcept
    {
        return operators_.find(op) != operators_.cend();
    }

    operator_info_type operator_info(std::string_view op) const
    {
        auto it = operators_.find(op);
        if (it == operators_.cend())
            return {};
        return it->second;
    }

    // use defaults for character types
    using tokeniser_state_machine_generic<cpp_tokeniser_state_machine>::is_oct_digit;
    using tokeniser_state_machine_generic<cpp_tokeniser_state_machine>::is_space;
    using tokeniser_state_machine_generic<cpp_tokeniser_state_machine>::is_numeric_digit;
    using tokeniser_state_machine_generic<cpp_tokeniser_state_machine>::is_operator_char;
    using tokeniser_state_machine_generic<cpp_tokeniser_state_machine>::is_quote_char;
    using tokeniser_state_machine_generic<cpp_tokeniser_state_machine>::is_token_separator;

  private:
    std::map<std::string_view, operator_info_type> operators_ = {
        { "*",{ operator_type::asterisk, "asterisk" } },
        { "&",{ operator_type::binary_or, "binary_or" } },
        { "!",{ operator_type::binary_not, "binary_not" } },
        { "!=", { operator_type::binary_not_eq, "binary_not_eq" } },
        { "<",{ operator_type::op_lt, "op_lt" } },
        { "<=", { operator_type::op_lte, "op_lte" } },
        { ">",{ operator_type::op_gt, "op_gt" } },
        { ">=", { operator_type::op_gte, "op_gte" } },
        { "=",{ operator_type::assignment, "assignment" } },
        { "==", { operator_type::equal, "equal" } },
        { "+",{ operator_type::plus, "plus" } },
        { "+=", { operator_type::plus_equal, "plus_equal" } },
        { "-",{ operator_type::minus, "minus" } },
        { "-=", { operator_type::minus_equal, "minus_equal" } },
        { "/",{ operator_type::divide, "divide" } },
        { "/=", { operator_type::divide_equal, "divide_equal" } },
        { "*=", { operator_type::multiply_equal, "multiply_equal" } },
        { ",",{ operator_type::comma, "comma" } },
        { ".",{ operator_type::period, "period" } },
        { "^",{ operator_type::power, "power" } },
        { "(",{ operator_type::open_paren, "open_paren" } },
        { ")",{ operator_type::close_paren, "close_paren" } },
        { "[",{ operator_type::open_sq_bracket, "open_sq_bracket" } },
        { "]",{ operator_type::close_sq_bracket, "close_sq_bracket" } },
        { "{",{ operator_type::open_curly_bracket, "open_curly_bracket" } },
        { "}",{ operator_type::close_curly_bracket, "close_curly_bracket" } },
        { ":",{ operator_type::colon, "colon" } },
        { ";",{ operator_type::semicolon, "semicolon" } },
        { "&&", { operator_type::logical_and, "logical_and" } },
        { "||", { operator_type::logical_or, "logical_or" } },
        { "<<", { operator_type::shift_left, "shift_left" } },
        { ">>", { operator_type::shift_right, "shift_right" } },
        { "++", { operator_type::increment, "increment" } },
        { "--", { operator_type::decrement, "decrement" } },
        { "::", { operator_type::scope_resolution, "scope_resolution" } },
    };
};

inline void run()
{
    cpp_tokeniser_state_machine cpptokeniser;
    std::vector<std::string> cpp_source = {
        "next.empty()",
        "operator<<",
        "operator>>()",
        "(*(++next))++",
        "int main()\n{\n}",
    };
    for (auto source : cpp_source)
        cpptokeniser.tokenise(source);

    bool quit = false;
    while (!quit)
    {
        std::string expression;
        std::getline(std::cin, expression);

        if (expression == "q"  ||  expression == "Q")
            quit = true;
        else if (!expression.empty()) {
            cpptokeniser.tokenise(expression);
            cpptokeniser.async_wait_for_state<tokeniser::states::token_complete>([](){});
        }
    }
}

}   // namespace cpp_tokeniser
