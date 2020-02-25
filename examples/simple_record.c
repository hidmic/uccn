#include "simple_record.h"

#include "uccn/uccn.h"
#include "mpack/mpack.h"

static void *simple_record_allocate(const struct uccn_record_typesupport_s * ts) {
  static struct simple_record_data_s buffer;
  (void)ts;
  return &buffer;
}

static ssize_t simple_record_serialize(const struct uccn_record_typesupport_s * ts,
                                       const struct simple_record_data_s * content,
                                       struct buffer_head_s * blob) {
  mpack_writer_t writer;
  (void)ts;
  mpack_writer_init(&writer, blob->data, blob->size);
  mpack_write_cstr(&writer, content->data);
  blob->length = mpack_writer_buffer_used(&writer);
  if (mpack_writer_destroy(&writer) != mpack_ok) {
    return -1;
  }
  return blob->length;
}

static ssize_t simple_record_deserialize(const struct uccn_record_typesupport_s * ts,
                                         const struct buffer_head_s * blob,
                                         struct simple_record_data_s * content) {
  mpack_reader_t reader;
  (void)ts;
  mpack_reader_init_data(&reader, blob->data, blob->length);
  mpack_expect_cstr(&reader, content->data, sizeof(content->data));
  if (mpack_reader_destroy(&reader) != mpack_ok) {
    return -1;
  }
  return blob->length;
}


struct uccn_record_typesupport_s simple_record_typesupport = {
  .allocate = (uccn_record_allocate_fn)simple_record_allocate,
  .serialize = (uccn_record_serialize_fn)simple_record_serialize,
  .deserialize = (uccn_record_deserialize_fn)simple_record_deserialize
};
