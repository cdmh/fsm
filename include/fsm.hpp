#pragma once

#include <chrono>
#include <cassert>
#include <deque>
#include <functional>   // std::bind
#include <iostream>
#include <mutex>
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
    // make the state machine non-copyable, non-movable
    state_machine(state_machine &&)                 = delete;
    state_machine &operator=(state_machine &&)      = delete;
    state_machine(state_machine const &)            = delete;
    state_machine &operator=(state_machine const &) = delete;

    state_machine()
    {
        event_thread_ = std::thread(std::bind(&state_machine::event_thread, this));
    }

    ~state_machine()
    {
        terminate_ = true;
        if (event_thread_.joinable())
            event_thread_.join();
    }

    void enqueue_event(event_t &&event)
    {
        std::scoped_lock lock(event_queue_mutex_);
        event_queue_.push_back(std::forward<event_t>(event));
    }

    void wait_for_empty_event_queue() const
    {
        using namespace std::literals::chrono_literals;

        while (true) {
            event_queue_mutex_.lock();
            bool empty = event_queue_.empty();
            event_queue_mutex_.unlock();

            if (empty)
                return;
            std::this_thread::sleep_for(5ms);
        }
    }

    template<typename State, typename Fn>
    void async_wait_for_state(Fn fn) const
    {
        using namespace std::literals::chrono_literals;

        std::thread([this, fn] {
            while (!std::holds_alternative<State>(current_state_))
                std::this_thread::sleep_for(10ms);
            fn();
        }).detach();
    }

  protected:
    state_t on_event(auto &&state, auto &&event)
    {
#ifndef NDEBUG
        std::ostringstream oss;
        oss << "\033[31mUnknown state/event combination\n";
        oss << "    " << typeid(state).name() << '\t' << typeid(state).raw_name() << '\n';
        oss << "    " << typeid(event).name() << '\t' << typeid(event).raw_name() << '\n';
        oss << "\033[0m";
        std::cout << oss.str();
#endif  // NDEBUG
        return std::move(state);
    }

    void process_event(event_t &&event)
    {
        auto fn = [instance = reinterpret_cast<derived_t *>(this)](auto &&state, auto &&event) -> state_t {
            if constexpr (DebugTrace) {
                std::ostringstream oss;
                oss << "\033[95m" << typeid(event).name() << "\033[0m\n    ";
                std::cout << oss.str();
            }

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

  private:
    void debug_output_transition(auto &new_state)
    {
        std::visit(
            [&new_state](auto &&type) {
                std::ostringstream oss;
                
                oss << "\033[93m"
                    << typeid(type).name() << " --> "
                    << typeid(new_state).name()
                    << "\033[0m\n";
                std::cout << oss.str();
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

    void event_thread()
    {
        using namespace std::literals::chrono_literals;

        while (!terminate_) {
            bool queue_empty = true;
            do {
                // empty() and pop() are both noexcept, so we don't need the
                // security of RAII, and using it would make the code less
                // readable with redundant scope blocks
                event_queue_mutex_.lock();
                queue_empty = event_queue_.empty();
                event_queue_mutex_.unlock();

                if (terminate_)
                    return;
                else if (queue_empty)
                    std::this_thread::sleep_for(5ms);
            } while (queue_empty);

            try {
                // process the event, leaving it in the queue so
                // other threads can wait on the queue being empty
                // to determine processing has finished in some
                // implementations
                event_t event(std::move(event_queue_.front()));
                process_event(std::move(event));

                event_queue_mutex_.lock();
                event_queue_.pop_front();
                event_queue_mutex_.unlock();
            }
            catch (std::exception &)
            {
            }
        }
    }

  private:
    bool                terminate_ = false;
    state_t             current_state_;
    std::thread         event_thread_;
    std::mutex mutable  event_queue_mutex_;
    std::deque<event_t> event_queue_;
};

}
