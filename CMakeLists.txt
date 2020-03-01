cmake_minimum_required(VERSION 3.5)

project(uccn)

add_subdirectory(vendor)

add_library(${PROJECT_NAME}
  src/uccn.c
  src/common/crc32.c
  src/utilities/upoll.c
  src/utilities/eventfd.c
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)

target_include_directories(${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor>
)

target_link_libraries(${PROJECT_NAME} mpack)

if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
