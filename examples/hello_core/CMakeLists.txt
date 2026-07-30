add_executable(hello_core_example main.cc)
target_link_libraries(hello_core_example PRIVATE stocas::hello_core stocas::project_options)
