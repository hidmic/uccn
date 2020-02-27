#include "simple_data.h"

#include "uccn/uccn.h"
#include "mpack/mpack.h"

static void * simple_data_allocate(const struct uccn_record_typesupport_s * ts) {
  static struct examples_simple_data_s buffer;
  (void)ts;
  return &buffer;
}

static ssize_t simple_data_serialize(const struct uccn_record_typesupport_s * ts,
                                     const struct examples_simple_data_s * content,
                                     struct buffer_head_s * blob) {
  mpack_writer_t writer;
  (void)ts;
  mpack_writer_init(&writer, blob->data, blob->size);
  mpack_write_u32(&writer, content->number);
  mpack_write_cstr(&writer, content->data);
  blob->length = mpack_writer_buffer_used(&writer);
  if (mpack_writer_destroy(&writer) != mpack_ok) {
    return -1;
  }
  return blob->length;
}

static ssize_t simple_data_deserialize(const struct uccn_record_typesupport_s * ts,
                                       const struct buffer_head_s * blob,
                                       struct examples_simple_data_s * content) {
  mpack_reader_t reader;
  (void)ts;
  mpack_reader_init_data(&reader, blob->data, blob->length);
  content->number = mpack_expect_u32(&reader);
  mpack_expect_cstr(&reader, content->data, sizeof(content->data));
  if (mpack_reader_destroy(&reader) != mpack_ok) {
    return -1;
  }
  return blob->length;
}

static struct uccn_record_typesupport_s g_simple_data_typesupport = {
  .allocate = (uccn_record_allocate_fn)simple_data_allocate,
  .serialize = (uccn_record_serialize_fn)simple_data_serialize,
  .deserialize = (uccn_record_deserialize_fn)simple_data_deserialize
};

const struct uccn_record_typesupport_s * get_examples_simple_data_typesupport(void) {
  return &g_simple_data_typesupport;
}
