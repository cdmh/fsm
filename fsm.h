#pragma once

#include <type_traits>
#include <variant>
#include <iostream> // !!!! DEBUG

namespace fsm {

namespace detail {

// std::visit helper
template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...)->overload<Ts...>;

//template <typename V, int ... Is>
//void print_variant_types_helper(std::integer_sequence<int, Is...> const &)
//{
//    using unused = int[];
//    (void)unused{
//        0,
//        (std::cout << typeid(std::variant_alternative_t<Is, V>).name() << '\n', 0)...};
//}
//
//template <typename V>
//void print_variant_types()
//{
//    print_variant_types_helper<V>(std::make_integer_sequence<int, std::variant_size_v<V>>{});
//}

}   // namespace detail

template<typename Derived, typename State, typename Event>
class state_machine
{
  private:
    using event_t = Event;
    using state_t = State;
    static_assert(std::is_copy_constructible_v<state_t>);

    using derived_t = Derived;

  public:
    state_machine(state_t &&initial_state)
    {
        set_current_state(std::forward<state_t>(initial_state));
    }

    void set_event(event_t const &ev)
    {
        auto instance = reinterpret_cast<derived_t *>(this);
        auto fn = [instance](auto const &state, auto const &evt) {
            return instance->on_event(state, evt);
        };

        set_current_state(
            std::visit(detail::overload{ fn }, std::move(current_state_), ev));
    }

  protected:
    state_t on_event(auto const &state, auto const &event)
    {
        std::cout << "Unknown state/event combination\n";
        std::cout << "    " << typeid(state).name() << '\t' << typeid(state).raw_name() << '\n';
        std::cout << "    " << typeid(event).name() << '\t' << typeid(event).raw_name() << '\n';
        return state;
    }

    void set_current_state(state_t &&state)
    {
        if (state.index() == current_state_.index())
            return;

        std::visit(
            detail::overload{[](auto &state){ state.leave(); }},
            current_state_);

        current_state_ = state;

        auto instance = reinterpret_cast<derived_t *>(this);
        std::visit(
            detail::overload{[instance](auto &state){ state.enter(*instance); } },
            current_state_);
    }

  private:
    state_t current_state_;
};

}
