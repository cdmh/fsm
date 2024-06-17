#pragma once

#include "include/fsm.hpp"
#include <array>
#include <cassert>
#include <sstream>

#ifndef TRACE_TOKENISER
#define TRACE_TOKENISER 0
#endif  // TRACE_TOKENISER

namespace tokeniser {

namespace detail {

namespace lut {

constexpr auto make_lut(std::string_view allowed)
{
    std::array<int, sizeof(std::string_view::value_type) << 8> lut{0};
    for (auto ch : allowed)
        lut[ch] = true;
    return lut;
}

constexpr auto valid_token_chars = make_lut("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_");
constexpr auto numeric_digits    = make_lut("0123456789");
constexpr auto space_chars       = make_lut(" \t\r\n\f\v");
constexpr auto operator_chars    = make_lut("*&!<>=+-/,.^()");
constexpr auto quote_chars       = make_lut("'\"");
constexpr auto bin_digits        = make_lut("01");
constexpr auto oct_digits        = make_lut("01234567");
constexpr auto hex_digits        = make_lut("0123456789ABCDEFabcdef");
    
constexpr
bool const is_valid_token_char(auto ch) noexcept
{
    return valid_token_chars[ch];
};

constexpr
bool const is_numeric_digit(auto ch) noexcept
{
    return numeric_digits[ch];
};

constexpr
bool const is_space(auto ch) noexcept
{
    return space_chars[ch];
};

constexpr
bool const is_operator_char(auto ch) noexcept
{
    return operator_chars[ch];
};

constexpr
bool const is_quote_char(auto ch) noexcept
{
    return quote_chars[ch];
};

constexpr
bool const is_hex_digit(auto ch) noexcept
{
    return hex_digits[ch];
};

constexpr
bool const is_oct_digit(auto ch) noexcept
{
    return oct_digits[ch];
};

constexpr
bool const is_bin_digit(auto ch) noexcept
{
    return bin_digits[ch];
};

constexpr
bool const is_token_separator(auto ch) noexcept
{
    return is_space(ch)  ||  is_operator_char(ch);
}

}   // namespace lut

using namespace lut;

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

  protected:
    expression_holder(std::string_view &&expression) noexcept
        : expr_(std::forward<std::string_view>(expression)),
          next_(expr_.cbegin())
    {
    }

    std::string_view::value_type const peek() const noexcept
    {
        return *next_;
    }

    std::string_view::value_type const next_char() noexcept
    {
        return *next_++;
    }

    bool const has_more_chars() const noexcept
    {
        return next_ != expr_.cend();
    }

    int64_t const position() const noexcept
    {
        return std::distance(expr_.cbegin(), next_);
    }

  protected://!!!
    std::string_view                 expr_;
    std::string_view::const_iterator next_;
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
  public:
    enum class token_type {
        unknown,
        numeric_literal,
        bin_literal,
        dec_literal,
        hex_literal,
        oct_literal,
        string_literal,
        operator_type,
        symbol,
    };

  public:
    token_info(expression_holder &&expression)
      : expression_holder(std::forward<expression_holder>(expression))
    {
    }

    void extend_token()
    {
        auto begin = token_.empty()? &*next_ : &*token_.cbegin();
        auto end   = (&*next_)+1;
        ++next_;
        token_ = std::string_view(begin, end);
    }

  public:
    token_type token_type_;
};

}   // namespace detail

// Event are transitions between states
namespace events {

class error
{
  public:
    error(std::string msg) : msg_(msg)
    {
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
struct seen_leading_zero  : public detail::token_info        { };
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
    seen_leading_zero,
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
            if (reinterpret_cast<Derived *>(this)->is_valid_char(peek())) {
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

class error
{
  public:
    error(std::string msg) : msg_(msg)
    {
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::cerr << "Error: " << msg_ << '\n';
        fsm.set_event(events::initialise());
    }

  private:
    std::string msg_;
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
        else if (detail::is_space(peek())) {
            next_char(); // consume whitespace
            fsm.set_event(events::begin_token(std::move(*this)));
        }
        else if (peek() == '.') {
            extend_token();
            fsm.set_event(events::to_dec_literal(std::move(*this)));
        }
        else if (detail::is_numeric_digit(peek())) {
            if (peek() == '0') {
                next_char(); // consume leading zero
                fsm.set_event(events::seen_leading_zero(std::move(*this)));
            }
            else {
                extend_token();
                fsm.set_event(events::seen_digit(std::move(*this)));
            }
        }
        else if (detail::is_operator_char(peek())) {
            extend_token();
            fsm.set_event(events::seen_operator_char(std::move(*this)));
        }
        else if (detail::is_quote_char(peek())) {
            extend_token();
            fsm.set_event(events::seen_quote(std::move(*this)));
        }
        else {
            extend_token();
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
#if TRACE_TOKENISER  ||  TRACE_TOKENS
        if (!token_.empty())
        {
            std::cout << "\033[96mFound token: \033[0m[";
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
                case token_type::operator_type:
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

        if (has_more_chars()) {
            if (token_type_ == token_type::operator_type
            ||  detail::is_token_separator(peek()))
            {
                fsm.set_event(events::begin_token(std::move(*this)));
            }
            else {
                std::ostringstream msg;
                msg << "Invalid character: '" << peek() << "' in \"" << expr() << "\" at position " << position();
                fsm.set_event(events::error(msg.str()));
            }
        }
        else
            fsm.set_event(events::initialise());
    }
};

class in_operator_token : public detail::in_token<in_operator_token>
{
  public:
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::operator_type;
        in_token<in_operator_token>::enter(fsm);
    }

    template<typename CharType>
    bool is_valid_char(CharType ch) const
    {
        return detail::is_operator_char(ch);
    }
};

class in_string_literal_token : public detail::in_token<in_string_literal_token>
{
  public:
    template<typename CharType>
    bool is_valid_char(CharType ch)
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
    template<typename CharType>
    bool is_valid_char(CharType ch) const
    {
        return detail::is_valid_token_char(ch);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::symbol;
        in_token::enter(fsm);
    }
};

class in_numeric_token : public detail::in_token<in_numeric_token>
{
    using base_t = in_token<in_numeric_token>;

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

            if (is_valid_char(peek())) {
                extend_token();
                fsm.set_event(events::continue_token(std::move(*this)));
                return;
            }
        }
        fsm.set_event(events::end_token(std::move(*this)));
    }

    template<typename CharType>
    bool is_valid_char(CharType ch) const
    {
        return detail::is_numeric_digit(ch);
    }
};

class in_numeric_base_token : public detail::in_token<in_numeric_base_token>
{
  public:
    template<typename CharType>
    bool is_valid_char(CharType ch) const
    {
        return (ch == 'b'  ||  ch == 'x'  ||  ch == '.'  ||  detail::is_oct_digit(ch));
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        if (peek() == 'b') {
            next_char();
            fsm.set_event(events::to_bin_literal(std::move(*this)));
            return;
        }
        else if (peek() == 'x') {
            next_char();
            fsm.set_event(events::to_hex_literal(std::move(*this)));
            return;
        }
        else if (peek() == '.') {
            fsm.set_event(events::to_dec_literal(std::move(*this)));
            return;
        }
        assert(detail::is_oct_digit(peek()));
        fsm.set_event(events::to_oct_literal(std::move(*this)));
    }
};

class in_bin_literal : public detail::in_token<in_bin_literal>
{
  public:
    template<typename CharType>
    bool is_valid_char(CharType ch) const
    {
        return detail::is_bin_digit(ch);
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
    template<typename CharType>
    bool is_valid_char(CharType ch) const
    {
        return detail::is_numeric_digit(ch);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        token_type_ = token_type::dec_literal;
        if (has_more_chars()  &&  peek() == '.')
        {
            std::ostringstream msg;
            msg << "Invalid character: '" << peek() << "' in \"" << expr() << "\" at position " << position();
            fsm.set_event(events::error(msg.str()));
        }
        else
            in_token::enter(fsm);
    }
};

class in_hex_literal : public detail::in_token<in_hex_literal>
{
  public:
    template<typename CharType>
    bool is_valid_char(CharType ch) const
    {
        return detail::is_hex_digit(ch);
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
    template<typename CharType>
    bool is_valid_char(CharType ch) const
    {
        return detail::is_oct_digit(ch);
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
    in_hex_literal,
    in_numeric_base_token,
    in_numeric_token,
    in_oct_literal,
    in_operator_token,
    in_string_literal_token,
    in_symbol_token,
    new_token,
    parse,
    token_complete
>;

}   // namespace states


class tokeniser_state_machine
    : public fsm::state_machine<tokeniser_state_machine, states::type, events::type, TRACE_TOKENISER==1>
{
  public:
    using base_t = fsm::state_machine<tokeniser_state_machine, states::type, events::type, TRACE_TOKENISER==1>;

    // enable default processing for undefined state/event pairs
    using base_t::on_event;

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

    states::type on_event(states::in_numeric_token &&, events::to_dec_literal &&event)
    {
        return states::in_dec_literal(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::new_token &&, events::to_dec_literal &&event)
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

    states::type on_event(auto &&, events::error &&event)
    {
        std::cout << "\033[91mERROR: " << event.what() << "\033[0m\n";
        return states::initialised();
    }
};

void run()
{
    tokeniser_state_machine tokeniser;

    std::vector<std::string> expressions = {
        "12.5",
        "2.5",
        ".5",
        "5.",
        "1234.6789.2",  // invalid
        "01",           // octal
        "12345",
        "678 ",
        " 90",
        " 123 456 ",
        "0x38afe",      // hex
        "0b101001",     // binary
        "01723",        // octal
        " 123x 45 678", // Invalid
        "123*0x2+ 0b10 / 19.234\t- 29^2",
    };
    for (auto expr : expressions)
        tokeniser.set_event(events::begin_parsing(expr));

    bool quit = false;
    while (!quit)
    {
        std::string expression;
        std::getline(std::cin, expression);

        if (expression == "q"  ||  expression == "Q")
            quit = true;
        else if (!expression.empty())
            tokeniser.set_event(
                events::begin_parsing(
                    std::string_view(
                        expression.cbegin(),
                        expression.cend())));
    }
}

}   // namespace tokeniser
