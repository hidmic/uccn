add_compile_options(-Wall -Wextra -Werror)

# C examples

add_library(example_resources_c simple_data.c)

target_link_libraries(example_resources_c ${PROJECT_NAME} mpack)


add_executable(simple_record_provider_c simple_record_provider.c)

target_link_libraries(simple_record_provider_c ${PROJECT_NAME} example_resources_c)

add_executable(simple_record_tracker_c simple_record_tracker.c)

target_link_libraries(simple_record_tracker_c ${PROJECT_NAME} example_resources_c)

# C++ examples

add_library(example_resources_cpp simple_data.cpp)

target_link_libraries(example_resources_cpp ${PROJECT_NAME} mpack)


add_executable(simple_record_provider_cpp simple_record_provider.cpp)

target_link_libraries(simple_record_provider_cpp ${PROJECT_NAME} example_resources_cpp)

add_executable(simple_record_tracker_cpp simple_record_tracker.cpp)

target_link_libraries(simple_record_tracker_cpp ${PROJECT_NAME} example_resources_cpp)
