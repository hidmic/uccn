add_library(mpack mpack/mpack.c)

target_include_directories(mpack
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/mpack>
)
