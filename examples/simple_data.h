#ifndef EXAMPLES_SIMPLE_DATA_H_
#define EXAMPLES_SIMPLE_DATA_H_

#include <stdint.h>

#include "uccn/uccn.h"

struct examples_simple_data_s
{
  uint32_t number;
  char data[64];
};

const struct uccn_record_typesupport_s * get_examples_simple_data_typesupport(void);


#endif  // EXAMPLES_SIMPLE_DATA_H_
