#pragma once

#include "tokeniser.hpp"

namespace cpp_tokeniser {

template<typename Derived>
class cpp_tokeniser_state_machine_generic
  : public tokeniser::tokeniser_state_machine_generic<Derived>
{
    using base_type = tokeniser::tokeniser_state_machine_generic<Derived>;

  public:
    enum class operator_type
    {
        assignment, asterisk,
        binary_not, binary_not_eq, binary_or,
        close_curly_bracket, close_paren, close_sq_bracket, colon, comma, comment,
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

    enum class keyword_type
    {
        kw_asm, kw_auto,
        kw_break,
        kw_case, kw_catch, kw_char, kw_class, kw_const, kw_continue,
        kw_default, kw_delete, kw_do, kw_double,
        kw_else, kw_enum, kw_extern,
        kw_float, kw_for, kw_friend,
        kw_goto,
        kw_if, kw_inline, kw_int,
        kw_long,
        kw_new,
        kw_operator,
        kw_private, kw_protected, kw_public,
        kw_register, kw_return,
        kw_short, kw_signed, kw_sizeof, kw_static, kw_struct, kw_switch,
        kw_template, kw_this, kw_throw, kw_try, kw_typedef,
        kw_union, kw_unsigned,
        kw_virtual, kw_void, kw_volatile,
        kw_while,

        // preprocessor_state_machine
        pp_define, pp_elif, pp_else, pp_endif, pp_error, pp_if, pp_ifdef,
        pp_ifndef, pp_import, pp_include, pp_line, pp_pragma, pp_undef,
        pp_using
    };
    using keyword_info_type  = keyword_type;

#ifdef _DEBUG
    cpp_tokeniser_state_machine_generic()
    {
        for (auto op : operators_) {
            for (auto ch : op.first)
                assert(is_operator_char(ch));
        }
    }
#endif

    bool const greedy_operator_check(std::string_view token) const noexcept
    {
        return operators_.find(token) != operators_.cend();
    }

    bool const is_keyword(std::string_view token) const noexcept
    {
        return keywords_.find(token) != keywords_.cend();
    }

    operator_info_type operator_info(std::string_view op) const
    {
        auto it = operators_.find(op);
        if (it == operators_.cend())
            return {};
        return it->second;
    }

    // use defaults for character types
    using base_type::is_oct_digit;
    using base_type::is_space;
    using base_type::is_numeric_digit;
    using base_type::is_operator_char;
    using base_type::is_quote_char;
    using base_type::is_token_separator;

  private:
    std::map<std::string_view, keyword_info_type> keywords_ = {
        { "asm",          keyword_type::kw_asm },
        { "auto",         keyword_type::kw_auto },
        { "break",        keyword_type::kw_break },
        { "case",         keyword_type::kw_case },
        { "catch",        keyword_type::kw_catch },
        { "char",         keyword_type::kw_char },
        { "class",        keyword_type::kw_class },
        { "const",        keyword_type::kw_const },
        { "continue",     keyword_type::kw_continue },
        { "default",      keyword_type::kw_default },
        { "delete",       keyword_type::kw_delete },
        { "do",           keyword_type::kw_do },
        { "double",       keyword_type::kw_double },
        { "else",         keyword_type::kw_else },
        { "enum",         keyword_type::kw_enum },
        { "extern",       keyword_type::kw_extern },
        { "float",        keyword_type::kw_float },
        { "for",          keyword_type::kw_for },
        { "friend",       keyword_type::kw_friend },
        { "goto",         keyword_type::kw_goto },
        { "if",           keyword_type::kw_if },
        { "inline",       keyword_type::kw_inline },
        { "int",          keyword_type::kw_int },
        { "long",         keyword_type::kw_long },
        { "new",          keyword_type::kw_new },
        { "operator",     keyword_type::kw_operator },
        { "private",      keyword_type::kw_private },
        { "protected",    keyword_type::kw_protected },
        { "public",       keyword_type::kw_public },
        { "register",     keyword_type::kw_register },
        { "return",       keyword_type::kw_return },
        { "short",        keyword_type::kw_short },
        { "signed",       keyword_type::kw_signed },
        { "sizeof",       keyword_type::kw_sizeof },
        { "static",       keyword_type::kw_static },
        { "struct",       keyword_type::kw_struct },
        { "switch",       keyword_type::kw_switch },
        { "template",     keyword_type::kw_template },
        { "this",         keyword_type::kw_this },
        { "throw",        keyword_type::kw_throw },
        { "try",          keyword_type::kw_try },
        { "typedef",      keyword_type::kw_typedef },
        { "union",        keyword_type::kw_union },
        { "unsigned",     keyword_type::kw_unsigned },
        { "virtual",      keyword_type::kw_virtual },
        { "void",         keyword_type::kw_void },
        { "volatile",     keyword_type::kw_volatile },
        { "while",        keyword_type::kw_while },
        { "#define",      keyword_type::pp_define },
        { "#elif",        keyword_type::pp_elif },
        { "#else",        keyword_type::pp_else },
        { "#endif",       keyword_type::pp_endif },
        { "#error",       keyword_type::pp_error },
        { "#if",          keyword_type::pp_if },
        { "#ifdef",       keyword_type::pp_ifdef },
        { "#ifndef",      keyword_type::pp_ifndef },
        { "#import",      keyword_type::pp_import },
        { "#include",     keyword_type::pp_include },
        { "#line",        keyword_type::pp_line },
        { "#pragma",      keyword_type::pp_pragma },
        { "#undef",       keyword_type::pp_undef },
        { "#using",       keyword_type::pp_using },
    };

    std::map<std::string_view, operator_info_type> operators_ = {
        { "*",  { operator_type::asterisk,            "asterisk"            } },
        { "&",  { operator_type::binary_or,           "binary_or"           } },
        { "!",  { operator_type::binary_not,          "binary_not"          } },
        { "!=", { operator_type::binary_not_eq,       "binary_not_eq"       } },
        { "<",  { operator_type::op_lt,               "op_lt"               } },
        { "<=", { operator_type::op_lte,              "op_lte"              } },
        { ">",  { operator_type::op_gt,               "op_gt"               } },
        { ">=", { operator_type::op_gte,              "op_gte"              } },
        { "=",  { operator_type::assignment,          "assignment"          } },
        { "==", { operator_type::equal,               "equal"               } },
        { "+",  { operator_type::plus,                "plus"                } },
        { "+=", { operator_type::plus_equal,          "plus_equal"          } },
        { "-",  { operator_type::minus,               "minus"               } },
        { "-=", { operator_type::minus_equal,         "minus_equal"         } },
        { "/",  { operator_type::divide,              "divide"              } },
        { "/=", { operator_type::divide_equal,        "divide_equal"        } },
        { "*=", { operator_type::multiply_equal,      "multiply_equal"      } },
        { ",",  { operator_type::comma,               "comma"               } },
        { ".",  { operator_type::period,              "period"              } },
        { "^",  { operator_type::power,               "power"               } },
        { "(",  { operator_type::open_paren,          "open_paren"          } },
        { ")",  { operator_type::close_paren,         "close_paren"         } },
        { "[",  { operator_type::open_sq_bracket,     "open_sq_bracket"     } },
        { "]",  { operator_type::close_sq_bracket,    "close_sq_bracket"    } },
        { "{",  { operator_type::open_curly_bracket,  "open_curly_bracket"  } },
        { "}",  { operator_type::close_curly_bracket, "close_curly_bracket" } },
        { ":",  { operator_type::colon,               "colon"               } },
        { "//", { operator_type::comment,             "comment"             } },
        { ";",  { operator_type::semicolon,           "semicolon"           } },
        { "&&", { operator_type::logical_and,         "logical_and"         } },
        { "||", { operator_type::logical_or,          "logical_or"          } },
        { "<<", { operator_type::shift_left,          "shift_left"          } },
        { ">>", { operator_type::shift_right,         "shift_right"         } },
        { "++", { operator_type::increment,           "increment"           } },
        { "--", { operator_type::decrement,           "decrement"           } },
        { "::", { operator_type::scope_resolution,    "scope_resolution"    } },
    };
};

class cpp_tokeniser_state_machine
  : public cpp_tokeniser_state_machine_generic<cpp_tokeniser_state_machine>
{
};

inline void run()
{
    cpp_tokeniser_state_machine cpptokeniser;
    std::vector<std::string> cpp_source = {
        "next.empty()",
        "operator<<",
        "operator>>()",
        "(*(++next))++",
        "int main(int const, char const * const)\n{\n}",
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
