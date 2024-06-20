//#define TRACE_TOKENISER 1
#define TRACE_TOKENS    1
#include "cpp_tokeniser.hpp"
#include "util/memory_mapped_file.hpp"

namespace cpp_preprocessor {

#ifdef _WIN32
namespace os = win32;
#endif  // _WIN32

namespace states {

class initialised
{
};

// this variant must include all states used in the state machine
using type = std::variant<
    initialised   // initial state
>;

}   // namespace states

namespace events {

// this variant must include all events used in the state machine
using type = std::variant<
>;

}   // namespace events

class cpp_preprocessor
{
  public:
    void run(std::string_view cpp)
    {
        cpp_tokeniser::cpp_tokeniser_state_machine tokeniser;
        tokeniser.tokenise(cpp);
    }
};

void run(char const * const pathname)
{
    auto mmf = os::map_file(pathname);
    if (!mmf) {
        std::cerr << "Unable to open file " << pathname << "\n";
        return;
    }

    cpp_preprocessor preprocessor;
    preprocessor.run(std::string_view(mmf, mmf.size()));
}

}   // namespace cpp_preprocessor
