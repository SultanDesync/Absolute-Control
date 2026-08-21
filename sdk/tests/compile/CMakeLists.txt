cmake_minimum_required(VERSION 3.25)
project(absolute_control_sdk_consumer LANGUAGES CXX)

add_executable(generated_menu_compile_test GeneratedMenuCompile.cpp)
target_compile_features(generated_menu_compile_test PRIVATE cxx_std_23)
target_include_directories(generated_menu_compile_test PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../../include"
    "${CMAKE_CURRENT_LIST_DIR}/../../examples/generated"
)
