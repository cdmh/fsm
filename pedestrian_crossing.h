#pragma once

#include "fsm.h"
#include <iostream>

namespace pedestrian_crossing {

namespace events {

    struct initialised
    {
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

}   // namespace events

using event = std::variant<
    events::initialised,
    events::press_button,
    events::timer<std::chrono::seconds>
>;

namespace states {

    struct state_machine;
    using namespace std::literals::chrono_literals;

    struct state_base
    {
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
        template<typename StateMachine>
        void enter(StateMachine &/*fsm*/)
        {
        }
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

}   // namespace states

using state = std::variant<
    states::initialising,   // initial state
    states::red,
    states::amber_flash,
    states::green,
    states::green_button_pressed,
    states::amber
>;

class crossing_state_machine : public fsm::state_machine<crossing_state_machine, state, event>
{
  public:
    using base_t = fsm::state_machine<crossing_state_machine, state, event>;

    // enable default processing for undefined state/event pairs
    using base_t::on_event;

    // GREEN state events
    state on_event(states::green const &state, events::press_button const &)
    {
        return states::green_button_pressed();
    }

    // GREEN, BUTTON PRESSED state events
    template<typename Duration>
    state on_event(states::green_button_pressed const &state, events::timer<Duration> const &)
    {
        return states::amber();
    }

    // AMBER state event
    template<typename Duration>
    state on_event(states::amber const &state, events::timer<Duration>)
    {
        return states::red();
    }

    // RED state events
    template<typename Duration>
    state on_event(states::red const &state, events::timer<Duration>)
    {
        return states::amber_flash();
    }

    state on_event(states::red const &state, events::press_button)
    {
        std::cout << "The lights are red, please cross the road now.\n";
        return state;
    }

    // AMBER FLASHING state events
    state on_event(states::amber_flash state, events::press_button)
    {
        using namespace std::literals::chrono_literals;

        auto timer_fn = [this] {
            wait_for_state(states::green());
            set_event(events::press_button());
        };
        std::thread(timer_fn).detach();
        std::cout << "Amber is flashing. Button press has been queued\n";
        return state;
    }

    template<typename Duration>
    state on_event(states::amber_flash const &state, events::timer<Duration>)
    {
        return states::green();
    }

    // all other states ignore the button press
    state on_event(auto const &state, events::press_button)
    {
        std::cout << "Button has already been pressed. Please be patient.\n";
        return state;
    }

    // INITIALISING state event
    state on_event(states::initialising const &state, events::initialised)
    {
        // initialise to Green state
        return states::green();
    }
};

}   // namespace pedestrian_crossing
