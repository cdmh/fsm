#include "fsm.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

namespace pedestrian_crossing {

namespace events {

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

        void leave()
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

        void leave() { }
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
        void leave() { }
    };

    struct green
    {
        template<typename StateMachine>
        void enter(StateMachine &/*fsm*/)
        {
            std::cout << "Lights are Green\n";
        }
        void leave() { }
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

        void leave() { }
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

        void leave() { }
    };

}   // namespace states

using state = std::variant<
    states::initialising,
    states::red,
    states::amber_flash,
    states::green,
    states::green_button_pressed,
    states::amber>;

using event = std::variant<
    events::press_button,
    events::timer<std::chrono::seconds>>;


class crossing_state_machine : public fsm::state_machine<crossing_state_machine, state, event>
{
  public:
    crossing_state_machine() : fsm::state_machine<crossing_state_machine, state, event>(states::green())
    {
    }

    // enable default processing for undefined state/event pairs
    using fsm::state_machine<crossing_state_machine, state, event>::on_event;

    state on_event(states::green const &state, events::press_button const &)
    {
        return states::green_button_pressed();
    }

    template<typename Duration>
    state on_event(states::green_button_pressed const &state, events::timer<Duration> const &)
    {
        return states::amber();
    }

    template<typename Duration>
    state on_event(states::amber const &state, events::timer<Duration>)
    {
        return states::red();
    }

    template<typename Duration>
    state on_event(states::red const &state, events::timer<Duration>)
    {
        return states::amber_flash();
    }

    state on_event(states::amber_flash const &state, events::press_button)
    {
        using namespace std::literals::chrono_literals;

        std::chrono::seconds duration = 25s;

        std::cout << "Amber is flashing. You'll have to wait " << duration << " now.\n";
        return states::green_button_pressed(duration);
    }

    template<typename Duration>
    state on_event(states::amber_flash const &state, events::timer<Duration>)
    {
        return states::green();
    }

    state on_event(states::red const &state, events::press_button)
    {
        std::cout << "The lights are red, please cross the road now.\n";
        return state;
    }

    // all other states ignore the button press
    state on_event(auto const &state, events::press_button)
    {
        std::cout << "Button has already been pressed. Please be patient.\n";
        return state;
    }
};

void run()
{
    crossing_state_machine crossing;

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

int main()
{
    pedestrian_crossing::run();
}
