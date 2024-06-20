//#define TRACE_TOKENISER 1
#define TRACE_TOKENS    1
#include "samples/pedestrian_crossing.hpp"
#include "samples/tokeniser.hpp"
#include "samples/cpp_tokeniser.hpp"

int main()
{
//    pedestrian_crossing::run();
    tokeniser::run();
    cpp_tokeniser::run();
}
