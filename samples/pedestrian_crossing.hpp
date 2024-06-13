#pragma once

#include "include/fsm.hpp"
#include <iostream>

namespace pedestrian_crossing {

// Event are transitions between states
namespace events {

struct initialised
{
    // make the state object noncopyable
    initialised()                               = default;
    initialised(initialised &&)                 = default;
    initialised &operator=(initialised &&)      = default;
    initialised(initialised const &)            = delete;
    initialised &operator=(initialised const &) = delete;
};

struct press_button
{
};

template<typename Duration>
struct timer
{
    timer(Duration duration) : duration(duration)
    {
    }

    Duration duration;
};
template<typename Duration>
timer<Duration> make_timer(Duration duration)
{
    return timer<Duration>(duration);
}

// this variant must include all events used in the state machine
using type = std::variant<
    initialised,
    press_button,
    timer<std::chrono::seconds>
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

struct state_base
{
    // make the state object noncopyable
    state_base()                              = default;
    state_base(state_base &&)                 = default;
    state_base &operator=(state_base &&)      = default;
    state_base(state_base const &)            = delete;
    state_base &operator=(state_base const &) = delete;

    template<typename StateMachine, typename Event>
    void transition_after_time(StateMachine &fsm, Event &&event)
    {
        auto timer_fn = [this, &fsm, event]{
            std::this_thread::sleep_for(event.duration);
            fsm.set_event(event);
        };
        std::thread(timer_fn).detach();
    }
};

struct initialising
{
};

struct red : state_base
{ 
    std::chrono::seconds duration = 15s;

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::cout << "Lights are Red\n";
        transition_after_time(fsm, events::make_timer(duration));
    }
};

struct green
{
    template<typename StateMachine>
    void enter(StateMachine &/*fsm*/)
    {
        std::cout << "Lights are Green\n";
    }
};

struct green_button_pressed : state_base
{
    std::chrono::seconds duration;

    green_button_pressed(std::chrono::seconds duration = 10s)
        : duration(duration)
    {
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::cout << "The lights are green and the button has been pressed. Please wait " << duration << '\n';
        transition_after_time(fsm, events::make_timer(duration));
    }
};

struct amber : state_base
{
    std::chrono::seconds duration = 2s;

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::cout << "Lights are Amber\n";
        transition_after_time(fsm, events::make_timer(duration));
    }
};

struct amber_flash : state_base
{
    std::chrono::seconds duration = 5s;

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::cout << "Lights are Flashing Amber\n";
        transition_after_time(fsm, events::make_timer(duration));
    }
};

struct amber_flash_button_pressed
{
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        using namespace std::literals::chrono_literals;

        std::cout << "Amber is flashing. Button press has been queued\n";
        fsm.async_wait_for_state(
            states::green(),
            [&fsm](){
                fsm.set_event(events::press_button());
            });
    }
};

using type = std::variant<
    initialising,   // initial state
    red,
    amber_flash,
    amber_flash_button_pressed,
    green,
    green_button_pressed,
    amber
>;

}   // namespace states

class crossing_state_machine
    : public fsm::state_machine<crossing_state_machine, states::type, events::type>
{
  public:
    using base_t = fsm::state_machine<crossing_state_machine, states::type, events::type>;

    // enable default processing for undefined state/event pairs
    using base_t::on_event;

    // GREEN state events
    states::type on_event(states::green &&state, events::press_button &&)
    {
        return states::green_button_pressed();
    }

    // GREEN, BUTTON PRESSED state events
    template<typename Duration>
    states::type on_event(states::green_button_pressed &&state, events::timer<Duration> &&)
    {
        return states::amber();
    }

    // AMBER state event
    template<typename Duration>
    states::type on_event(states::amber &&state, events::timer<Duration> &&)
    {
        return states::red();
    }

    // RED state events
    template<typename Duration>
    states::type on_event(states::red &&state, events::timer<Duration> &&)
    {
        return states::amber_flash();
    }

    states::type on_event(states::red &&state, events::press_button &&)
    {
        std::cout << "The lights are red, please cross the road now.\n";
        return std::move(state);
    }

    // AMBER FLASHING state events
    states::type on_event(states::amber_flash &&, events::press_button &&)
    {
        return states::amber_flash_button_pressed();
    }

    template<typename Duration>
    states::type on_event(states::amber_flash &&state, events::timer<Duration> &&)
    {
        return states::green();
    }

    // AMBER FLASHING, BUTTON PRESSED state events
    template<typename Duration>
    states::type on_event(states::amber_flash_button_pressed &&state, events::timer<Duration> &&)
    {
        return states::green();
    }

    // all other states ignore the button press
    states::type on_event(auto &&state, events::press_button &&)
    {
        std::cout << "Button has already been pressed. Please be patient.\n";
        return std::move(state);
    }

    // INITIALISING state event
    states::type on_event(states::initialising &&state, events::initialised &&)
    {
        // initialise to Green state
        return states::green();
    }
};

void run()
{
    crossing_state_machine crossing;
    crossing.set_event(events::initialised());

    bool quit = false;
    while (!quit)
    {
        char ch;
        std::cin.get(ch);

        switch (ch)
        {
            
            case 'q':
            case 'Q':
                quit = true;
                break;

            case 'b':
            case 'B':
                crossing.set_event(events::press_button());
                break;
        }
    }
}

}   // namespace pedestrian_crossing
