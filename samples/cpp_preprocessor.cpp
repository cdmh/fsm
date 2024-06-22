#define TRACE_TOKENISER 0
#define TRACE_TOKENS    1
#define TRACE_CPP_PREPROCESSOR 1
#include "cpp_tokeniser.hpp"
#include "util/memory_mapped_file.hpp"

namespace cpp_preprocessor {

#ifdef _WIN32
namespace os = win32;
#endif  // _WIN32

enum class keyword_type
{
    pp_none,
    pp_preprocessor,    // '#' on column 1
    pp_define, pp_elif, pp_else, pp_endif, pp_error, pp_if, pp_ifdef,
    pp_ifndef, pp_import, pp_include, pp_line, pp_pragma, pp_undef,
    pp_using
};


namespace events {

struct on_token       : public tokeniser::token_info { };
struct seen_directive : public tokeniser::token_info { };


// this variant must include all events used in the state machine
using type = std::variant<
    tokeniser::events::end_token,
    on_token,
    seen_directive
>;

}   // namespace events

namespace states {

class initialised
{
};

class receive_token : public tokeniser::token_info
{
  public:
    template<typename StateMachine>
    void reenter(StateMachine &fsm)
    {
        enter(fsm);
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        std::ostringstream oss;
        oss << "(" << column() << ", " << line() << ") " << "\033[30;46m" << token_ << "\033[0m\n";
        std::cout << oss.str();

        if (column() == 1  &&  fsm.look_keyword(token_) == keyword_type::pp_preprocessor)
            fsm.set_event(events::seen_directive(std::move(*this)));
    }
};

class directive : public tokeniser::token_info
{
  public:
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        auto keyword = fsm.look_keyword(token_);
        switch (keyword)
        {
            case keyword_type::pp_include:  assert(true);//fsm.set_event(events::seen_include(std::move(*this)));  break;
        }
    }
};

// this variant must include all states used in the state machine
using type = std::variant<
    initialised,    // initial state
    receive_token,
    directive
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
        return states::receive_token(std::forward<decltype(event)>(event));
    }

    states::type on_event(auto &&state, events::on_token &&event)
    {
        return decltype(state)(std::forward<decltype(event)>(event));
    }

    states::type on_event(states::receive_token &&, events::seen_directive &&event)
    {
        return states::directive(std::forward<decltype(event)>(event));
    }

    keyword_type const look_keyword(std::string_view token) const noexcept
    {
        auto const it = keywords_.find(token);
        return (it == keywords_.cend())? keyword_type::pp_none : it->second;
    }

  private:
    using keyword_info_type  = keyword_type;

    std::map<std::string_view, keyword_info_type> keywords_ = {
        { "define",      keyword_type::pp_define       },
        { "elif",        keyword_type::pp_elif         },
        { "else",        keyword_type::pp_else         },
        { "endif",       keyword_type::pp_endif        },
        { "error",       keyword_type::pp_error        },
        { "if",          keyword_type::pp_if           },
        { "ifdef",       keyword_type::pp_ifdef        },
        { "ifndef",      keyword_type::pp_ifndef       },
        { "import",      keyword_type::pp_import       },
        { "include",     keyword_type::pp_include      },
        { "line",        keyword_type::pp_line         },
        { "pragma",      keyword_type::pp_pragma       },
        { "undef",       keyword_type::pp_undef        },
        { "using",       keyword_type::pp_using        },
        { "#",           keyword_type::pp_preprocessor },
    };
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
        preprocessor_.wait_for_empty_event_queue();
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
