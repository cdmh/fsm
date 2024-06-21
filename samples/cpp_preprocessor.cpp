#define TRACE_TOKENISER 0
#define TRACE_TOKENS    1
#define TRACE_CPP_PREPROCESSOR 1
#include "cpp_tokeniser.hpp"
#include "util/memory_mapped_file.hpp"

namespace cpp_preprocessor {

#ifdef _WIN32
namespace os = win32;
#endif  // _WIN32

namespace events {

struct on_token : public tokeniser::token_info { };


// this variant must include all events used in the state machine
using type = std::variant<
    tokeniser::events::end_token,
    on_token
>;

}   // namespace events

namespace states {

class initialised
{
};

class process_token : public tokeniser::token_info
{
  public:
    template<typename StateMachine>
    void entry(StateMachine &fsm)
    {
    }
};

// this variant must include all states used in the state machine
using type = std::variant<
    initialised,   // initial state
    process_token
>;

}   // namespace states

namespace detail {

class preprocessor_state_machine
  : public fsm::state_machine<preprocessor_state_machine, states::type, events::type, TRACE_CPP_PREPROCESSOR==1>
{
    using base_type = fsm::state_machine<preprocessor_state_machine, states::type, events::type, TRACE_CPP_PREPROCESSOR==1>;

  public:
    // enable default processing for undefined state/event pairs
    using base_type::on_event;

    states::type on_event(states::initialised &&, events::on_token &&event)
    {
        return states::process_token(std::forward<decltype(event)>(event));
    }
};

}   // namespace detail

class preprocessor
  : public cpp_tokeniser::cpp_tokeniser_state_machine_generic<preprocessor>
{
    using base_type = cpp_tokeniser_state_machine_generic<preprocessor>;

  public:
    void run(std::string_view cpp)
    {
        tokenise(cpp);
    }

    // enable default processing for undefined state/event pairs
    using base_type::on_event;

    // we receive an event from the tokeniser state machine to enter this one
    tokeniser::states::type on_event(auto &&state, tokeniser::events::end_token &&event)
    {
        // copy the token here so we own a token for our state machine
        tokeniser::token_info token(event);
        preprocessor_.set_event(events::on_token(std::move(token)));

        // the return type needs to be from the same state machine, so we call the
        // base class to maintain that state machine while we change states on our
        // this machine
        return base_type::on_event(
            std::forward<decltype(state)>(state),
            std::forward<decltype(event)>(event));
    }

  private:
    detail::preprocessor_state_machine preprocessor_;
};

void run(char const * const pathname)
{
    auto mmf = os::map_file(pathname);
    if (!mmf) {
        std::cerr << "Unable to open file " << pathname << "\n";
        return;
    }

    preprocessor pp;
    pp.run(std::string_view(mmf, mmf.size()));
}

}   // namespace preprocessor
