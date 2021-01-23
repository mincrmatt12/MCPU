cmake_minimum_required(VERSION 3.15)
project(mcpu)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")

add_subdirectory(core)
add_subdirectory(assembler)
