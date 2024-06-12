#pragma once

#include "include/fsm.hpp"
#include <iostream>

namespace tokeniser {

// Event are transitions between states
namespace events {

class parse
{
  public:
    parse(std::string_view &&expression)
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

// this variant must include all events used in the state machine
using type = std::variant<
    events::parse
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

struct initialising
{
};

class parse
{
    public:
    parse(std::string_view expression)
        : expr_(expression)
    {
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::cout << "Ready to parse: " << expr_ << "\n";
    }

    private:
    std::string_view expr_;
};

using type = std::variant<
    states::initialising,   // initial state
    states::parse
>;

}   // namespace states

class tokeniser_state_machine
    : public fsm::state_machine<tokeniser_state_machine, states::type, events::type>
{
  public:
    using base_t = fsm::state_machine<tokeniser_state_machine, states::type, events::type>;

    // enable default processing for undefined state/event pairs
    using base_t::on_event;

    states::type on_event(states::initialising &&, events::parse &&parse)
    {
        return states::parse(parse.expression());
    }
};

void run()
{
    tokeniser_state_machine tokeniser;

    tokeniser.set_event(events::parse("100*0x2+0b11/19.234-29^2"));

    bool quit = false;
    while (!quit)
    {
        std::string expression;
        std::getline(std::cin, expression);

        if (expression == "q\n"  ||  expression == "Q\n")
            quit = true;
        else
            tokeniser.set_event(
                events::parse(
                    std::string_view(
                        expression.cbegin(),
                        expression.cend())));
    }
}

}   // namespace tokeniser
