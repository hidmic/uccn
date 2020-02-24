cmake_minimum_required(VERSION 3.5)

project(uccn C)

include_directories(include vendor)

add_subdirectory(vendor)

add_library(${PROJECT_NAME}
  src/uccn.c
  src/common/hash.c
  src/utilities/upoll.c
  src/utilities/eventfd.c
)

target_link_libraries(${PROJECT_NAME} mpack)

add_subdirectory(examples)
