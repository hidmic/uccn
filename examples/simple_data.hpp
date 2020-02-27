#ifndef EXAMPLES_SIMPLE_DATA_HPP_
#define EXAMPLES_SIMPLE_DATA_HPP_

#include <stdint.h>

#include "uccn/uccn.hpp"

namespace examples {

struct simple_data
{
  static const uccn_record_typesupport_s * get_typesupport();

  uint32_t number;
  char data[64];
};

}  // namespace examples

#endif  // EXAMPLES_SIMPLE_DATA_HPP_
