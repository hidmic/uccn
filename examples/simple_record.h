#ifndef SIMPLE_RECORD_H_
#define SIMPLE_RECORD_H_

struct simple_record_data_s
{
  char data[64];
};

extern struct uccn_record_typesupport_s simple_record_typesupport;

#endif  // SIMPLE_RECORD_H_
