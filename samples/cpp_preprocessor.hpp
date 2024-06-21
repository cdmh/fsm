#pragma once

#include "cpp_tokeniser.hpp"

namespace cpp_preprocessor {

class cpp_preprocessor : public cpp_tokeniser::cpp_tokeniser_state_machine
{
  public:
};

void run(char const * const pathname);

}   // namespace preprocessor
