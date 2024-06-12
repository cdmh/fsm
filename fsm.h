#pragma once

#include <chrono>
#include <thread>
#include <type_traits>
#include <variant>
#ifndef NDEBUG
#include <iostream>
#endif  // NDEBUG

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

template<typename Derived, typename State, typename Event>
class state_machine
{
  private:
    using event_t = Event;
    using state_t = State;
    using derived_t = Derived;

  public:
    state_machine() = default;

    void set_event(event_t const &ev)
    {
        auto instance = reinterpret_cast<derived_t *>(this);
        auto fn = [instance](auto const &state, auto const &evt) {
            return instance->on_event(state, evt);
        };

        auto &&new_state = std::visit(detail::overload{ fn }, std::move(current_state_), ev);
        if (new_state.index() == current_state_.index())
            return;

        std::visit(
            detail::overload{[this](auto &state){ leave(state); }},
            current_state_);

        current_state_ = std::forward<state_t>(new_state);

        std::visit(
            detail::overload{[this](auto &state){ enter(state); } },
            current_state_);
    }

    void wait_for_state(state_t const &state) const
    {
        using namespace std::literals::chrono_literals;

        while (current_state_.index() != state.index())
            std::this_thread::sleep_for(10ms);
    }

  protected:
    state_t on_event(auto const &state, auto const &event)
    {
#ifndef NDEBUG
        std::cout << "Unknown state/event combination\n";
        std::cout << "    " << typeid(state).name() << '\t' << typeid(state).raw_name() << '\n';
        std::cout << "    " << typeid(event).name() << '\t' << typeid(event).raw_name() << '\n';
#endif  // NDEBUG
        return state;
    }

  private:
    // make the state machine noncopyable
    state_machine(state_machine const &)            = delete;
    state_machine& operator=(state_machine const &) = delete;

    template<typename State>
    void enter(State &state)
    {
        auto instance = reinterpret_cast<derived_t *>(this);
        if constexpr (requires { state.enter(*this); })
            state.enter(*this);
    }

    template<typename State>
    void leave(State state)
    {
        auto instance = reinterpret_cast<derived_t *>(this);
        if constexpr (requires { state.leave(*this); })
            state.leave(*this);
    }

  private:
    state_t current_state_;
};

}
