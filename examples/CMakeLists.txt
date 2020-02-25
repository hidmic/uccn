add_compile_options(-Wall -Wextra -Werror)

add_library(simple_resources simple_record.c)

target_link_libraries(simple_resources ${PROJECT_NAME} mpack)

add_executable(simple_record_provider
  simple_record_provider.c
)

target_link_libraries(simple_record_provider ${PROJECT_NAME} simple_resources)

add_executable(simple_record_tracker
  simple_record_tracker.c
)

target_link_libraries(simple_record_tracker ${PROJECT_NAME} simple_resources)
