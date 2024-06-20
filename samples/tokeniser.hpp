#pragma once

#include "include/fsm.hpp"
#include <array>
#include <cassert>
#include <map>
#include <sstream>

#ifndef TRACE_TOKENISER
#define TRACE_TOKENISER 0
#endif  // TRACE_TOKENISER

namespace tokeniser {

namespace detail {

class expression_holder
{
  public:
    expression_holder()                                     = delete;
    expression_holder(expression_holder &&)                 = default;
    expression_holder &operator=(expression_holder &&)      = default;
    expression_holder(expression_holder const &)            = delete;
    expression_holder &operator=(expression_holder const &) = delete;

    std::string_view expr() const noexcept
    {
        return expr_;
    }

    int64_t const line() const noexcept
    {
        return line_;
    }

    int64_t const column() const noexcept
    {
        return column_;
    }

  protected:
    expression_holder(std::string_view &&expression) noexcept
        : expr_(std::forward<std::string_view>(expression)),
          next_(expr_.cbegin())
    {
    }

    std::string_view::value_type const &peek() const noexcept
    {
        return *next_;
    }

    std::string_view::value_type const next_char() noexcept
    {
        ++column_;
        return *next_++;
    }

    uint64_t const on_next_line() noexcept
    {
        column_ = 0;
        return ++line_;
    }

    bool const has_more_chars() const noexcept
    {
        return next_ != expr_.cend();
    }

    int64_t const position() const noexcept
    {
        return std::distance(expr_.cbegin(), next_);
    }

  private:
    std::string_view                 expr_;
    std::string_view::const_iterator next_;
    int64_t                          line_   = 1;
    int64_t                          column_ = 0;
};

class token_holder
{
  public:
    token_holder() noexcept                       = default;
    token_holder(token_holder &&)                 = default;
    token_holder &operator=(token_holder &&)      = default;
    token_holder(token_holder const &)            = delete;
    token_holder &operator=(token_holder const &) = delete;

    token_holder(std::string_view token) noexcept
      : token_(token)
    {
    }

  protected:
    std::string_view token_;
};

class token_info : public expression_holder, public token_holder
{
  protected:
    enum class token_type {
        unknown,
        numeric_literal,
        bin_literal,
        dec_literal,
        hex_literal,
        oct_literal,
        string_literal,
        operator_token,
        symbol,
    };

  protected:
    token_info(expression_holder &&expression)
      : expression_holder(std::forward<expression_holder>(expression))
    {
    }

    void extend_token()
    {
        token_ = peek_extend_token();
        next_char();
    }

    std::string_view peek_extend_token() const
    {
        auto start = &(peek());
        auto begin = token_.empty()? start : &*token_.cbegin();
        auto end   = start+1;
        return std::string_view(begin, end);
    }

    void reset_token()
    {
        token_ = std::string_view();
    }

  protected:
    token_type token_type_ = token_type::unknown;
};

}   // namespace detail

// Event are transitions between states
namespace events {

class error : public detail::expression_holder
{
  public:
    error(expression_holder &&other)
      : expression_holder(std::forward<expression_holder>(other))
    {
    }

    void set_message(std::string msg)
    {
        msg_ = msg;
    }

    char const * const what() const noexcept
    {
        return msg_.c_str();
    }

  private:
    std::string msg_;
};

struct begin_parsing : public detail::expression_holder
{
    begin_parsing(std::string_view &&expression)
        : expression_holder(std::forward<std::string_view>(expression))
    {
    }
};

struct initialise                                            { };
struct begin_token        : public detail::expression_holder { };
struct continue_token     : public detail::token_info        { };
struct end_token          : public detail::token_info        { };
struct seen_digit         : public detail::token_info        { };
struct seen_exponent      : public detail::token_info        { };
struct seen_leading_zero  : public detail::token_info        { };
struct seen_newline       : public detail::token_info        { };
struct seen_operator_char : public detail::token_info        { };
struct seen_quote         : public detail::token_info        { };
struct seen_symbol_char   : public detail::token_info        { };
struct to_bin_literal     : public detail::token_info        { };
struct to_dec_literal     : public detail::token_info        { };
struct to_hex_literal     : public detail::token_info        { };
struct to_oct_literal     : public detail::token_info        { };

// this variant must include all events used in the state machine
using type = std::variant<
    begin_parsing,
    begin_token,
    continue_token,
    end_token,
    error,
    initialise,
    seen_digit,
    seen_exponent,
    seen_leading_zero,
    seen_newline,
    seen_operator_char,
    seen_quote,
    seen_symbol_char,
    to_bin_literal,
    to_dec_literal,
    to_hex_literal,
    to_oct_literal
>;

}   // namespace events


namespace detail {

template<typename Derived>
class in_token : public token_info
{
  public:
    in_token(token_info &&other)
      : token_info(std::forward<token_info>(other))
    {
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        if (has_more_chars()) {
            if (reinterpret_cast<Derived *>(this)->is_valid_char(fsm, peek())) {
                extend_token();
                fsm.set_event(events::continue_token(std::move(*this)));
                return;
            }
        }
        fsm.set_event(events::end_token(std::move(*this)));
    }

    template<typename StateMachine>
    void reenter(StateMachine &fsm)
    {
        static_cast<Derived *>(this)->enter(fsm);
    }
};

}   // namespace detail


// Each state is implemented in a structure and can hold state
// while it is active
// Functions enter() and leave() are option, but if exist they
// will be call by the finite state machine class during state
// transitions
namespace states {

struct state_machine;
using namespace std::literals::chrono_literals;

class initialised
{
};

class error : public detail::expression_holder
{
  public:
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::cerr << "Error: " << msg_ << '\n';
        fsm.set_event(events::initialise());
    }

  private:
    std::string msg_;
};

class new_line : public detail::token_info
{
  public:
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        on_next_line();
        fsm.set_event(events::begin_token(std::move(*this)));
    }
};

class new_token : public detail::token_info
{
  public:
    new_token(expression_holder &&other)
      : token_info(std::forward<expression_holder>(other))
    {
    }

    template<typename StateMachine>
    void reenter(StateMachine &fsm)
    {
        enter(fsm);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        if (!has_more_chars()) {
            fsm.set_event(events::end_token(std::move(*this)));
        }
        else if (peek() == '\n') {
            next_char(); // consume whitespace
            fsm.set_event(events::seen_newline(std::move(*this)));
        }
        else if (fsm.is_space(peek())) {
            next_char(); // consume whitespace
            fsm.set_event(events::begin_token(std::move(*this)));
        }
        else {
            auto const ch = peek();
            extend_token();

            if (ch == '.') {
                if (has_more_chars()  &&  fsm.is_numeric_digit(peek()))
                    fsm.set_event(events::to_dec_literal(std::move(*this)));
                else
                    fsm.set_event(events::seen_operator_char(std::move(*this)));
            }
            else if (ch == '0')
                fsm.set_event(events::seen_leading_zero(std::move(*this)));
            else if (fsm.is_numeric_digit(ch))
                fsm.set_event(events::seen_digit(std::move(*this)));
            else if (fsm.is_operator_char(ch))
                fsm.set_event(events::seen_operator_char(std::move(*this)));
            else if (fsm.is_quote_char(ch))
                fsm.set_event(events::seen_quote(std::move(*this)));
            else
                fsm.set_event(events::seen_symbol_char(std::move(*this)));
        }
    }
};

class parse : public detail::expression_holder
{
  public:
    parse(std::string_view &&expression)
      : expression_holder(std::forward<std::string_view>(expression))
    {
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
#if TRACE_TOKENISER  ||  TRACE_TOKENS
        std::cout << "\n\033[92mParsing: \"" << expr() << "\"\033[0m\n";
#endif  // TRACE_TOKENISER
        fsm.set_event(events::begin_token(std::move(*this)));
    }
};

class token_complete : public detail::token_info
{
  public:
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        if (has_more_chars()
        &&  token_type_ != token_type::operator_token
        &&  !fsm.is_token_separator(peek()))
        {
            std::ostringstream msg;
            msg << "Invalid character: '" << next_char() << "' in \"" << expr() << "\" at position " << position();
            events::error err(std::move(*this));
            err.set_message(msg.str());
            fsm.set_event(std::move(err));
            return;
        }
        
        write_token_info(fsm);

        if (has_more_chars())
            fsm.set_event(events::begin_token(std::move(*this)));
        else
            fsm.set_event(events::initialise());
    }

    template<typename StateMachine>
    void write_token_info(StateMachine &fsm) const
    {
#if TRACE_TOKENISER  ||  TRACE_TOKENS
        if (!token_.empty())
        {
            std::cout << "\033[96m" << expr() << "\033[0m\t[";
            switch (token_type_) {
                case token_type::unknown:
                    std::cout << "unknown type";
                    break;
                case token_type::numeric_literal:
                    std::cout << "numeric";
                    break;
                case token_type::bin_literal:
                    std::cout << "binary literal";
                    break;
                case token_type::dec_literal:
                    std::cout << "decimal literal";
                    break;
                case token_type::hex_literal:
                    std::cout << "hex literal";
                    break;
                case token_type::oct_literal:
                    std::cout << "octal literal";
                    break;
                case token_type::string_literal:
                    std::cout << "string";
                    break;
                case token_type::operator_token:
                    if constexpr (requires { fsm.operator_info(token_); })
                        std::cout << "operator \033[93m" << fsm.operator_info(token_).second << "\033[0m";
                    else
                        std::cout << "operator";
                    break;
                case token_type::symbol:
                    std::cout << "symbol";
                    break;
                default:
                    std::cout << "UNDEFINED";
                    break;
            }
            std::cout << "] \033[30;46m" << token_ << "\033[0m";
            switch (token_type_) {
                case token_type::bin_literal:
                {
                    char* ptr = nullptr;
                    std::cout << " (binary, decimal value " << strtoll(&*token_.cbegin(), &ptr, 2) << ')';
                    break;
                }
                case token_type::dec_literal:
                {
                    std::string token(token_);
                    std::istringstream iss(token);
                    double value;
                    iss >> value;
                    std::cout << " (decimal value " << value << ')';
                    break;
                }
                case token_type::hex_literal:
                {
                    char* ptr = nullptr;
                    std::cout << " (hex, decimal value " << strtoll(&*token_.cbegin(), &ptr, 16) << ')';
                    break;
                }
                case token_type::oct_literal:
                {
                    char* ptr = nullptr;
                    std::cout << " (octal, decimal value " << strtoll(&*token_.cbegin(), &ptr, 8) << ')';
                    break;
                }
            }
            std::cout << "\n";
        }
#endif  // TRACE_TOKENISER
    }
};

class in_operator_token : public detail::in_token<in_operator_token>
{
  public:
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::operator_token;

        if constexpr (requires { fsm.greedy_operator_check(peek_extend_token()); }) {
            if (has_more_chars()) {
                if (fsm.greedy_operator_check(peek_extend_token())) {
                    extend_token();
                    fsm.set_event(events::continue_token(std::move(*this)));
                    return;
                }
            }
        }
        fsm.set_event(events::end_token(std::move(*this)));
    }

};

class in_string_literal_token : public detail::in_token<in_string_literal_token>
{
  public:
    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &, CharType ch)
    {
        if (ch == token_[0]) {
            extend_token();
            return false;
        }
        return true;
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::string_literal;
        in_token::enter(fsm);
    }
};

class in_symbol_token : public detail::in_token<in_symbol_token>
{
  public:
    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &fsm, CharType ch) const
    {
        return fsm.is_valid_token_char(ch);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::symbol;
        in_token::enter(fsm);
    }
};

class in_exponent: public detail::in_token<in_exponent>
{
  public:
    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &fsm, CharType ch) const
    {
        // allow a sign for the exponent
        if (*token_.crbegin() == 'e'  &&  (ch == '-'  ||  ch == '+'))
            return true;
        return fsm.is_numeric_digit(ch);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::dec_literal;
        in_token::enter(fsm);
    }
};

class in_numeric_token : public detail::in_token<in_numeric_token>
{
  public:
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::numeric_literal;

        if (has_more_chars()) {
            if (peek() == '.') {
                extend_token();
                fsm.set_event(events::to_dec_literal(std::move(*this)));
                return;
            }

            if (peek() == 'e'  ||  peek() == 'E') {
                extend_token();
                fsm.set_event(events::seen_exponent(std::move(*this)));
                return;
            }

            if (is_valid_char(fsm, peek())) {
                extend_token();
                fsm.set_event(events::continue_token(std::move(*this)));
                return;
            }
        }
        fsm.set_event(events::end_token(std::move(*this)));
    }

    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &fsm, CharType ch) const
    {
        return fsm.is_numeric_digit(ch);
    }
};

class in_numeric_base_token : public detail::in_token<in_numeric_base_token>
{
  public:
    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &, CharType ch) const
    {
        return (ch == 'b'  ||  ch == 'x'  ||  ch == '.'  ||  fsm.is_oct_digit(ch));
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        assert(token_.length() == 1  &&  token_[0] == '0');

        token_type_ = token_type::numeric_literal;
        if (has_more_chars()) {
            auto const ch = peek();
            switch (ch) {
                case 'b':
                    // clear the leading 0 and skip the base indicator
                    reset_token();
                    next_char();
                    fsm.set_event(events::to_bin_literal(std::move(*this)));
                    return;
        
                case 'x':
                    // clear the leading 0 and skip the base indicator
                    reset_token();
                    next_char();
                    fsm.set_event(events::to_hex_literal(std::move(*this)));
                    return;
        
                case 'e':
                case 'E':
                    extend_token();
                    fsm.set_event(events::seen_exponent(std::move(*this)));
                    return;
        
                case '.':
                    extend_token();
                    fsm.set_event(events::to_dec_literal(std::move(*this)));
                    return;

                default:        
                    if (fsm.is_oct_digit(peek())) {
                        fsm.set_event(events::to_oct_literal(std::move(*this)));
                        return;
                    }
            }
        }

        fsm.set_event(events::end_token(std::move(*this)));
    }
};

class in_bin_literal : public detail::in_token<in_bin_literal>
{
  public:
    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &fsm, CharType ch) const
    {
        return fsm.is_bin_digit(ch);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::bin_literal;
        in_token::enter(fsm);
    }
};

class in_dec_literal : public detail::in_token<in_dec_literal>
{
  public:
    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &fsm, CharType ch) const
    {
        return fsm.is_numeric_digit(ch);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::dec_literal;

        if (has_more_chars()) {
            if (peek() == 'e'  ||  peek() == 'E') {
                extend_token();
                fsm.set_event(events::seen_exponent(std::move(*this)));
                return;
            }
            else if (peek() == '.') {
                std::ostringstream msg;
                msg << "Invalid character: '" << next_char() << "' in \"" << expr() << "\" at position " << position();
                events::error err(std::move(*this));
                err.set_message(msg.str());
                fsm.set_event(std::move(err));
                return;
            }
        }
        in_token::enter(fsm);
    }
};

class in_hex_literal : public detail::in_token<in_hex_literal>
{
  public:
    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &fsm, CharType ch) const
    {
        return fsm.is_hex_digit(ch);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::hex_literal;
        in_token::enter(fsm);
    }
};

class in_oct_literal : public detail::in_token<in_oct_literal>
{
  public:
    template<typename StateMachine, typename CharType>
    bool is_valid_char(StateMachine const &fsm, CharType ch) const
    {
        return fsm.is_oct_digit(ch);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::oct_literal;
        in_token::enter(fsm);
    }
};

using type = std::variant<
    initialised,   // initial state
    error,
    in_bin_literal,
    in_dec_literal,
    in_exponent,
    in_hex_literal,
    in_numeric_base_token,
    in_numeric_token,
    in_oct_literal,
    in_operator_token,
    in_string_literal_token,
    in_symbol_token,
    new_line,
    new_token,
    parse,
    token_complete
>;

}   // namespace states

static constexpr auto make_lut(std::string_view allowed)
{
    std::array<int, sizeof(std::string_view::value_type) << 8> lut{0};
    for (auto ch : allowed)
        lut[ch] = true;
    return lut;
}

template<typename Derived>
class tokeniser_state_machine_generic
  : public fsm::state_machine<Derived, states::type, events::type, TRACE_TOKENISER==1>
{
  public:
    using base_type = fsm::state_machine<Derived, states::type, events::type, TRACE_TOKENISER==1>;

    void tokenise(std::string_view str)
    {
        base_type::set_event(events::begin_parsing(std::move(str)));
        base_type::wait_for_empty_event_queue();
    }

    static constexpr auto valid_token_chars = make_lut("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_");
    static constexpr auto numeric_digits    = make_lut("0123456789");
    static constexpr auto space_chars       = make_lut(" \t\r\n\f\v");
    static constexpr auto operator_chars    = make_lut("*|&!<>=+-/,.^()[]{}:;");
    static constexpr auto quote_chars       = make_lut("'\"");
    static constexpr auto bin_digits        = make_lut("01");
    static constexpr auto oct_digits        = make_lut("01234567");
    static constexpr auto hex_digits        = make_lut("0123456789ABCDEFabcdef");
    
    constexpr
    bool const is_valid_token_char(auto ch) const noexcept
    {
        return valid_token_chars[ch];
    };

    constexpr
    bool const is_numeric_digit(auto ch) const noexcept
    {
        return numeric_digits[ch];
    };

    constexpr
    bool const is_space(auto ch) const noexcept
    {
        return space_chars[ch];
    };

    constexpr
    bool const is_operator_char(auto ch) const noexcept
    {
        return operator_chars[ch];
    };

    constexpr
    bool const is_quote_char(auto ch) const noexcept
    {
        return quote_chars[ch];
    };

    constexpr
    bool const is_hex_digit(auto ch) const noexcept
    {
        return hex_digits[ch];
    };

    constexpr
    bool const is_oct_digit(auto ch) const noexcept
    {
        return oct_digits[ch];
    };

    constexpr
    bool const is_bin_digit(auto ch) const noexcept
    {
        return bin_digits[ch];
    };

    constexpr
    bool const is_token_separator(auto ch) const noexcept
    {
        return is_space(ch)  ||  is_operator_char(ch);
    }
    
    // enable default processing for undefined state/event pairs
    using base_type::on_event;

    states::type on_event(auto &&, events::initialise &&)
    {
        return states::initialised();
    }

    states::type on_event(states::initialised &&, events::begin_parsing &&parse)
    {
        return states::parse(parse.expr());
    }

    states::type on_event(states::new_token &&, events::seen_digit &&event)
    {
        return states::in_numeric_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&, events::seen_leading_zero &&event)
    {
        return states::in_numeric_base_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&, events::seen_operator_char &&event)
    {
        return states::in_operator_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&, events::seen_quote &&event)
    {
        return states::in_string_literal_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&, events::seen_symbol_char &&event)
    {
        return states::in_symbol_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&, events::seen_newline &&event)
    {
        return states::new_line(std::forward<decltype(event)>(event));
    }

    states::type on_event(auto &&state, events::continue_token &&event)
    {
        using state_t = decltype(state);
        return state_t(event);
    }

    states::type on_event(auto &&, events::end_token &&event)
    {
        return states::token_complete(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::parse &&, events::begin_token &&event)
    {
        return states::new_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&state, events::begin_token &&event)
    {
        return states::new_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&state, events::end_token &&event)
    {
        return states::token_complete(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&, events::to_dec_literal &&event)
    {
        return states::in_dec_literal(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_line &&state, events::begin_token &&event)
    {
        return states::new_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::token_complete &&state, events::begin_token &&event)
    {
        return states::new_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::in_numeric_base_token &&, events::to_bin_literal &&event)
    {
        return states::in_bin_literal(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::in_numeric_base_token &&, events::to_dec_literal &&event)
    {
        return states::in_dec_literal(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::in_numeric_base_token &&, events::to_hex_literal &&event)
    {
        return states::in_hex_literal(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::in_numeric_base_token &&, events::to_oct_literal &&event)
    {
        return states::in_oct_literal(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::in_numeric_base_token &&, events::seen_exponent &&event)
    {
        return states::in_exponent(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::in_numeric_token &&, events::to_dec_literal &&event)
    {
        return states::in_dec_literal(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::in_numeric_token &&, events::seen_exponent &&event)
    {
        return states::in_exponent(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::in_dec_literal &&, events::seen_exponent &&event)
    {
        return states::in_exponent(std::forward<decltype(event)>(event));
    }

    states::type on_event(auto &&, events::error &&event)
    {
        std::cout << "\033[91mERROR on line " << event.line() << ", column " << event.column() << ": " << event.what() << "\033[0m\n";
        return states::initialised();
    }
};


class state_machine : public tokeniser_state_machine_generic<state_machine>
{
};

inline void run()
{
    state_machine tokeniser;

    std::vector<std::string> expressions = {
        "0",
        "0.3",
        "4e3",
        ".3e4",
        "0e3",
        ".1e-12",
        ".1e+8",
        "12.5",
        "2.5",
        ".5",
        "5.",
        "01",           // octal
        "12345",
        "678 ",
        " 90",
        " 123 456 ",
        "1+2",
        "0x38afe",      // hex
        "0b101001",     // binary
        "01723",        // octal
        "123*0x2+ 0b10 / 19.234\t- 29^2",
        // invalids
        "1234.6789.2",
        " 123x 45 678",
        "++",
    };

    std::cout << "Generic tokeniser\n";
    for (auto expr : expressions)
        tokeniser.tokenise(expr);
}

}   // namespace tokeniser
