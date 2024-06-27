#define TRACE_TOKENISER 0
#define TRACE_TOKENS    1

#define _CRT_SECURE_NO_WARNINGS 1
#include "samples/pedestrian_crossing.hpp"
#include "samples/tokeniser.hpp"
#include "samples/cpp_preprocessor.hpp"

int main_()
{
//    pedestrian_crossing::run();
//    tokeniser::run();
//    cpp_tokeniser::run();
    cpp_preprocessor::run(__FILE__);
    return 0;
}
