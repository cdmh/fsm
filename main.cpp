//define TRACE_FSM 1
//#define TRACE_TOKENISER 1
#define TRACE_TOKENS    1
#include "samples/pedestrian_crossing.hpp"
#include "samples/tokeniser.hpp"
#include "samples/cpp_preprocessor.hpp"

int main()
{
//    pedestrian_crossing::run();
//    tokeniser::run();
//    cpp_tokeniser::run();
    cpp_preprocessor::run(__FILE__);
}
