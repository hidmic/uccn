cmake_minimum_required(VERSION 3.5)

project(uccn)

include_directories(include vendor)

add_subdirectory(vendor)

add_library(${PROJECT_NAME}
  src/uccn.c
  src/common/hash.c
  src/utilities/upoll.c
  src/utilities/eventfd.c
)

target_link_libraries(${PROJECT_NAME} mpack)

if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
