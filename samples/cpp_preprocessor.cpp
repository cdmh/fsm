#define TRACE_TOKENISER 0
#define TRACE_TOKENS    1
#define TRACE_CPP_PREPROCESSOR 1
#define _CRT_SECURE_NO_WARNINGS 1
#include <filesystem>
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

struct initialise                                           { };
struct on_token              : public tokeniser::token_info { };
struct seen_directive        : public tokeniser::token_info { };
struct seen_include          : public tokeniser::token_info { };
struct include_open_bracket  : public tokeniser::token_info { };
struct include_close_bracket : public tokeniser::token_info { };

// this variant must include all events used in the state machine
using type = std::variant<
    tokeniser::events::end_token,
    include_open_bracket,
    include_close_bracket,
    initialise,
    on_token,
    seen_directive,
    seen_include
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

        if (column() == 1  &&  fsm.lookup_keyword(token_) == keyword_type::pp_preprocessor)
            fsm.set_event(events::seen_directive(std::move(*this)));
    }
};

class directive : public tokeniser::token_info
{
  public:
    template<typename StateMachine>
    void reenter(StateMachine &fsm)
    {
        auto const keyword = fsm.lookup_keyword(token_);
        switch (keyword)
        {
            case keyword_type::pp_include:  fsm.set_event(events::seen_include(std::move(*this)));  break;
        }
    }

    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        assert(fsm.lookup_keyword(token_) == keyword_type::pp_preprocessor);
    }
};

class include : public tokeniser::token_info
{
  public:
    template<typename StateMachine>
    void enter(StateMachine &fsm)
    {
        assert(fsm.lookup_keyword(token_) == keyword_type::pp_include);
    }

    template<typename StateMachine>
    void reenter(StateMachine &fsm)
    {
        if (token_type_ == token_type::string_literal) {
            assert(token_.front() == '\"');
            assert(token_.back() == '\"');

            std::string filename(std::string_view(++token_.begin(), --token_.end()));
            fsm.run(filename);
        }
        else if (token_type_ == token_type::operator_token) {
            assert(token_.front() == '<');
            fsm.set_event(events::include_open_bracket(std::move(*this)));
        }
    }
};

class include_from_path : public tokeniser::token_info
{
  public:
    template<typename StateMachine>
    void reenter(StateMachine &fsm)
    {
        std::filesystem::path pathname = std::string_view(token_.begin(), token_.end());

        std::string includes;
        if (auto p = getenv("INCLUDE"))
            includes = p;

        if (!includes.empty()) {
            size_t start = 0;
            do
            {
#ifdef _WIN32
                auto const sep = ';';
#else
                auto const sep = ':';
#endif
                auto end = includes.find(sep, start);
                std::filesystem::path fullpath = includes.substr(start, end) / pathname;
                if (exists(fullpath)  &&  !is_directory(fullpath)) {
                    pathname = std::move(fullpath);
                    break;
                }
                else if (end == std::string::npos)
                    return;

                start = end+1;
            } while (true);
        }

        fsm.run(pathname.string());
        fsm.set_event(events::include_close_bracket(std::move(*this)));
    }
};

class end_include_from_path : public tokeniser::token_info
{
  public:
    template<typename StateMachine>
    void reenter(StateMachine &fsm)
    {
        assert(token_.back() == '>');
        fsm.set_event(events::initialise());
    }
};

// this variant must include all states used in the state machine
using type = std::variant<
    initialised,    // initial state
    directive,
    end_include_from_path,
    include,
    include_from_path,
    receive_token
>;

}   // namespace states

namespace detail {

template<typename PP>
class preprocessor_tokeniser
  : public cpp_tokeniser::cpp_tokeniser_state_machine_generic<preprocessor_tokeniser<PP>>
{
    using base_type = cpp_tokeniser::cpp_tokeniser_state_machine_generic<preprocessor_tokeniser<PP>>;

  public:
    preprocessor_tokeniser(PP &preprocessor) : preprocessor_(preprocessor)
    {
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
    PP &preprocessor_;
};

}   // namespace detail

class preprocessor
  : protected fsm::state_machine<preprocessor, states::type, events::type, TRACE_TOKENISER==1>
{
    using base_type = fsm::state_machine<preprocessor, states::type, events::type, TRACE_TOKENISER==1>;

  public:
    // enable default processing for undefined state/event pairs
    using base_type::on_event;

    states::type on_event(auto &&, events::include_open_bracket  &&event)
    {
        return states::include_from_path(std::forward<decltype(event)>(event));
    }

    states::type on_event(auto &&, events::include_close_bracket &&event)
    {
        return states::end_include_from_path(std::forward<decltype(event)>(event));
    }

    states::type on_event(auto &&, events::initialise &&)
    {
        return states::initialised();
    }

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

    states::type on_event(states::directive &&, events::seen_include &&event)
    {
        return states::include(std::forward<decltype(event)>(event));
    }

    keyword_type const lookup_keyword(std::string_view token) const noexcept
    {
        auto const it = keywords_.find(token);
        return (it == keywords_.cend())? keyword_type::pp_none : it->second;
    }

    void set_event(events::type &&event)
    {
        process_event(std::forward<events::type>(event));
    }

    void run(std::string_view pathname)
    {
        set_event(events::initialise());

#ifdef TRACE_CPP_PREPROCESSOR
        std::cout << "\033[95mInclude " << pathname << "\033[0m\n";
#endif  // TRACE_CPP_PREPROCESSOR

        auto mmf = os::map_file(pathname);
        if (!mmf) {
            std::cerr << "Unable to open file \"" << pathname << "\". Error " << GetLastError() << "\n";
            return;
        }

        detail::preprocessor_tokeniser<preprocessor> tokeniser(*this);
        tokeniser.tokenise(std::string_view(mmf, mmf.size()));
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

void run(char const * const pathname)
{
    preprocessor pp;
    pp.run(pathname);
}

}   // namespace preprocessor
