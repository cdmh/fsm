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
    // make the state machine noncopyable, but keep it movable
    state_machine()                                 = default;
    state_machine(state_machine &&)                 = default;
    state_machine &operator=(state_machine &&)      = default;
    state_machine(state_machine const &)            = delete;
    state_machine &operator=(state_machine const &) = delete;

    void set_event(event_t &&event)
    {
        auto instance = reinterpret_cast<derived_t *>(this);
        auto fn = [instance](auto &&state, auto &&event) -> state_t {
            return instance->on_event(
                std::move(state),
                std::move(event));
        };

        auto &&new_state = std::visit(detail::overload{ fn }, current_state_, event);
        if (new_state.index() == current_state_.index())
            return;

        std::visit(
            detail::overload{[this](auto &state){ leave(state); }},
            current_state_);

        current_state_ = std::move(new_state);

        std::visit(
            detail::overload{[this](auto &state){ enter(state); } },
            current_state_);
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
        std::cout << "Unknown state/event combination\n";
        std::cout << "    " << typeid(state).name() << '\t' << typeid(state).raw_name() << '\n';
        std::cout << "    " << typeid(event).name() << '\t' << typeid(event).raw_name() << '\n';
#endif  // NDEBUG
        //static_assert(!"Error");
        return std::move(state);
    }

  private:
    template<typename State>
    void enter(State &state)
    {
        auto instance = reinterpret_cast<derived_t *>(this);
        if constexpr (requires { state.enter(*instance); })
            state.enter(*instance);
    }

    template<typename State>
    void leave(State &state)
    {
        auto instance = reinterpret_cast<derived_t *>(this);
        if constexpr (requires { state.leave(*instance); })
            state.leave(*instance);
    }

  private:
    state_t current_state_;
};

}
