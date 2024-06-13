#pragma once

#include "include/fsm.hpp"
#include <iostream>

namespace detail {

class expression_holder
{
  public:
    expression_holder()                                     = delete;
    expression_holder(expression_holder &&)                 = default;
    expression_holder &operator=(expression_holder &&)      = default;
    expression_holder(expression_holder const &)            = delete;
    expression_holder &operator=(expression_holder const &) = delete;

    std::string_view &&extract_expr() noexcept
    {
        return std::move(expr_);
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

    std::string_view expr() const noexcept
    {
        return expr_;
    }

    bool has_more_chars() const noexcept
    {
        return next_ != expr_.cend();
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

    token_holder(std::string &&token) noexcept
        : token_(std::forward<std::string>(token))
    {
    }

    std::string &&extract_token() noexcept
    {
        return std::move(token_);
    }

  protected:
    std::string token_;
};

class token_info : public expression_holder, public token_holder
{
  public:
    token_info()                              = delete;
    token_info(token_info &&)                 = default;
    token_info &operator=(token_info &&)      = default;
    token_info(token_info const &)            = delete;
    token_info &operator=(token_info const &) = delete;

    token_info(std::string_view &&expression)
      : expression_holder(std::forward<std::string_view>(expression))
    {
    }

    token_info(std::string_view &&expression, std::string &&token)
      : expression_holder(std::forward<std::string_view>(expression)),
        token_holder(std::forward<std::string>(token))
    {
    }
};

}   // namespace detail

namespace tokeniser {

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

class initialise
{
};

class begin_parsing
{
  public:
    begin_parsing(std::string_view &&expression)
      : expr_(expression)
    {
    }

    std::string_view expression() const
    {
        return expr_;
    }

    private:
    std::string_view expr_;
};

class begin_token : public detail::expression_holder
{
  public:
    begin_token(std::string_view &&expression)
      : expression_holder(std::forward<std::string_view>(expression))
    {
    }
};

class continue_token : public detail::token_info
{
  public:
    continue_token(token_info &&other)
      : token_info(std::forward<token_info>(other))
    {
    }
};

// this variant must include all events used in the state machine
using type = std::variant<
    error,
    initialise,
    begin_parsing,
    begin_token,
    continue_token
>;

}   // namespace events


// Each state is implemented in a structure and can hold state
// while it is active
// Functions enter() and leave() are option, but if exist they
// will be call by the finite state machine class during state
// transitions
namespace states {

struct state_machine;
using namespace std::literals::chrono_literals;

struct initialised
{
    template<typename StateMachine>
    void leave(StateMachine &)
    {
        std::cout << "Leaving Initialised\n";
    }
};

struct error
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

class in_token : public detail::token_info
{
  public:
    in_token(token_info &&other)
      : token_info(std::forward<token_info>(other))
    {
    }

    in_token(std::string_view &&expression, std::string &&token)
      : token_info(std::forward<std::string_view>(expression), std::forward<std::string>(token))
    {
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
    std::cout << (void*)token_.c_str() << '\t' <<  (void*)&*next_ << ' ' << *next_ << '\n';
        if (std::isdigit(peek())) {
            token_ += next_char();
            fsm.set_event(events::continue_token(std::move(*this)));
        }
        else if (has_more_chars())
            fsm.set_event(events::error("Digit expected!"));
    }

    template<typename StateMachine>
    void reenter(StateMachine &fsm)
    {
        enter(fsm);
    }
};

struct new_token : public detail::token_info
{
  public:
    new_token(std::string_view &&expression)
      : token_info(std::forward<std::string_view>(expression))
    {
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
    std::cout << (void*)token_.c_str() << '\t' <<  (void*)&*next_ << ' ' << *next_ << '\n';
        if (std::isdigit(peek())) {
            token_ += next_char();
            events::continue_token event(std::move(*this));
            fsm.set_event(std::move(event));
        }
        else if (has_more_chars())
            fsm.set_event(events::error("Digit expected!"));
    }
};

class parse : detail::expression_holder
{
  public:
    parse(std::string_view &&expression)
      : expression_holder(std::forward<std::string_view>(expression))
    {
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::cout << "Ready to begin_parsing: " << expr() << "\n";
        fsm.set_event(events::begin_token(expr()));
    }
};

using type = std::variant<
    initialised,   // initial state
    parse,
    new_token,
    in_token,
    error
>;

}   // namespace states

class tokeniser_state_machine
    : public fsm::state_machine<tokeniser_state_machine, states::type, events::type, true>
{
  public:
    using base_t = fsm::state_machine<tokeniser_state_machine, states::type, events::type, true>;

    // enable default processing for undefined state/event pairs
    using base_t::on_event;

    states::type on_event(states::error &&, events::initialise &&)
    {
        return states::initialised();
    }

    states::type on_event(states::initialised &&, events::begin_parsing &&parse)
    {
        return states::parse(parse.expression());
    }

    states::type on_event(states::parse, events::begin_token &&event)
    {
        return states::new_token(std::move(event.extract_expr()));
    }

    states::type on_event(states::new_token, events::continue_token &&event)
    {
        return states::in_token(std::forward<events::continue_token>(event));
    }

    states::type on_event(states::in_token &&state, events::continue_token &&event)
    {
        return state;
    }
};

void run()
{
    tokeniser_state_machine tokeniser;

    std::vector<std::string> expressions = {
        "10032",
        "100*0x2+0b11/19.234-29^2",
    };
    for (auto expr : expressions)
        tokeniser.set_event(events::begin_parsing(expr));

    bool quit = false;
    while (!quit)
    {
        std::string expression;
        std::getline(std::cin, expression);

        if (expression == "q\n"  ||  expression == "Q\n")
            quit = true;
        else
            tokeniser.set_event(
                events::begin_parsing(
                    std::string_view(
                        expression.cbegin(),
                        expression.cend())));
    }
}

}   // namespace tokeniser
