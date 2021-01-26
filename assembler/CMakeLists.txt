# todo: find assembler parser stuffies

find_package(BISON REQUIRED)
find_package(RE2C REQUIRED)

re2c_target(NAME mcasm_re2c INPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parser.y.re)
bison_target(mcasm_yacc ${CMAKE_CURRENT_BINARY_DIR}/parser.y.re ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)

file(GLOB assembler_srcs src/*.cpp)
add_executable(mcasm ${assembler_srcs} ${BISON_mcasm_yacc_OUTPUTS})

set_target_properties(mcasm PROPERTIES
	CXX_STANDARD 20
)

target_include_directories(mcasm PRIVATE ${CMAKE_CURRENT_BINARY_DIR} src)

install(TARGETS mcasm RUNTIME DESTINATION bin)
