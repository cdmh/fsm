#include "catch.hpp"

#define TRACE_TOKENS           1
#define TRACE_TOKENISER        1
#define TRACE_CPP_PREPROCESSOR 1
#include "../samples/cpp_preprocessor.hpp"

TEST_CASE("Preprocessor symbol definitions", "[symbols]")
{
    cpp_preprocessor::preprocessor pp;
    cpp_preprocessor::detail::preprocessor_tokeniser<cpp_preprocessor::preprocessor> tokeniser(pp);

    REQUIRE(tokeniser.tokenise("#define a"));
    REQUIRE(pp.ifdef("a"));

    REQUIRE(tokeniser.tokenise("#undef a"));
    REQUIRE(!pp.ifdef("a"));

    REQUIRE(tokeniser.tokenise("#define a\n#undef a"));
    REQUIRE(!pp.ifdef("a"));

    //REQUIRE(cpp_preprocessor::run(__FILE__));
}
