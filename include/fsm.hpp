#pragma once

#include <chrono>
#include <iostream>
#include <thread>
#include <type_traits>
#include <variant>

namespace fsm {

namespace detail {

// std::visit helper
template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...)->overload<Ts...>;

#ifndef NDEBUG
template <typename V, int ... Is>
void print_variant_types_helper(std::integer_sequence<int, Is...> const &)
{
    using unused = int[];
    (void)unused{
        0,
        (std::cout << typeid(std::variant_alternative_t<Is, V>).name() << '\n', 0)...};
}

template <typename V>
void print_variant_types()
{
    print_variant_types_helper<V>(std::make_integer_sequence<int, std::variant_size_v<V>>{});
}
#endif  // NDEBUG

}   // namespace detail

template<typename Derived, typename State, typename Event, bool DebugTrace=false>
class state_machine
{
  private:
    using event_t = Event;
    using state_t = State;
    using derived_t = Derived;

  public:
    // make the state machine noncopyable, but keep it movable
    state_machine()                                 = default;
    state_machine(state_machine &&)                 = default;
    state_machine &operator=(state_machine &&)      = default;
    state_machine(state_machine const &)            = delete;
    state_machine &operator=(state_machine const &) = delete;

    void set_event(event_t &&event)
    {
        auto fn = [instance = reinterpret_cast<derived_t *>(this)](auto &&state, auto &&event) -> state_t {
            if constexpr (DebugTrace)
                std::cout << "\033[95m" << typeid(event).name() << "\033[0m\n    ";

            return instance->on_event(
                std::move(state),
                std::move(event));
        };

        auto &&new_state = std::visit(detail::overload{ fn }, current_state_, event);

        bool const state_changed = (new_state.index() != current_state_.index());
        std::visit(
            detail::overload{[this, state_changed](auto &&state){ transition(state, state_changed); } },
            new_state);
    }

    template<typename Fn>
    void async_wait_for_state(state_t const &state, Fn fn) const
    {
        using namespace std::literals::chrono_literals;

        std::thread([this, &state, fn] {
            while (current_state_.index() != state.index())
                std::this_thread::sleep_for(10ms);
            fn();
        }).detach();
    }

  protected:
    state_t on_event(auto &&state, auto &&event)
    {
#ifndef NDEBUG
        std::cout << "\033[31mUnknown state/event combination\n";
        std::cout << "    " << typeid(state).name() << '\t' << typeid(state).raw_name() << '\n';
        std::cout << "    " << typeid(event).name() << '\t' << typeid(event).raw_name() << '\n';
        std::cout << "\033[0m";
#endif  // NDEBUG
        return std::move(state);
    }

  private:
    void debug_output_transition(auto &new_state)
    {
        std::visit(
            [&new_state](auto &&type) {
                std::cout << "\033[93m"
                          << typeid(type).name() << " --> "
                          << typeid(new_state).name()
                          << "\033[0m\n";
            },
            current_state_);
    }
    void transition(auto &&new_state, bool const state_changed)
    {
        if constexpr (DebugTrace)
            debug_output_transition(new_state);

        auto instance = reinterpret_cast<derived_t *>(this);
        if (state_changed)
        {
            std::visit(
                [this](auto &state){
                    auto instance = reinterpret_cast<derived_t *>(this);
                    if constexpr (requires { state.leave(*instance); })
                        state.leave(*instance);
                },
                current_state_);

            current_state_ = std::move(new_state);

            std::visit(
                [this](auto &state){
                    auto instance = reinterpret_cast<derived_t *>(this);
                    if constexpr (requires { state.enter(*instance); })
                        state.enter(*instance);
                },
                current_state_);
        }
        else
        {
            // if the state hasn't changed, we call reenter() instead of
            // leave() followed by enter().
            // this gives the FSM implementer a choice of how to handle
            // re-entry
            if constexpr (requires { new_state.reenter(*instance); })
                new_state.reenter(*instance);
        }
    }

  private:
    state_t current_state_;
};

}
