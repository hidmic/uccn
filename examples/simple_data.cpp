#include "simple_data.hpp"

#include "mpack/mpack.h"

#include "uccn/uccn.hpp"

namespace examples {
namespace {

void *simple_data_allocate(const uccn_record_typesupport_s * ts) {
  static simple_data buffer;
  (void)ts;
  return &buffer;
}

ssize_t simple_data_serialize(const uccn_record_typesupport_s * ts,
                              const simple_data * content,
                              buffer_head_s * blob) {
  mpack_writer_t writer;
  (void)ts;
  mpack_writer_init(&writer, static_cast<char *>(blob->data), blob->size);
  mpack_write_u32(&writer, content->number);
  mpack_write_cstr(&writer, content->data);
  blob->length = mpack_writer_buffer_used(&writer);
  if (mpack_writer_destroy(&writer) != mpack_ok) {
    return -1;
  }
  return blob->length;
}

ssize_t simple_data_deserialize(const uccn_record_typesupport_s * ts,
                                const buffer_head_s * blob,
                                simple_data * content) {
  mpack_reader_t reader;
  (void)ts;
  mpack_reader_init_data(&reader, static_cast<char *>(blob->data), blob->length);
  content->number = mpack_expect_u32(&reader);
  mpack_expect_cstr(&reader, content->data, sizeof(content->data));
  if (mpack_reader_destroy(&reader) != mpack_ok) {
    return -1;
  }
  return blob->length;
}

uccn_record_typesupport_s g_simple_data_typesupport = {
  .allocate = (uccn_record_allocate_fn)simple_data_allocate,
  .serialize = (uccn_record_serialize_fn)simple_data_serialize,
  .deserialize = (uccn_record_deserialize_fn)simple_data_deserialize
};

}  // namespace

const uccn_record_typesupport_s * simple_data::get_typesupport() {
  return &g_simple_data_typesupport;
}

}  // namespace examples
