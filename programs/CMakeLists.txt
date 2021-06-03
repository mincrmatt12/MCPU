# Define macro for building file
find_program(C_PREPROCESSOR cpp REQUIRED)

set(LIB_PATH ${CMAKE_CURRENT_LIST_DIR}/../lib)
file(GLOB_RECURSE LIB_DEPS ${LIB_PATH}/*.inc)

macro(make_mcpu_bin TARGETNAME SOURCEFILE)
	# Preprocess
	file(GLOB_RECURSE ${TARGETNAME}_FILES ${CMAKE_CURRENT_LIST_DIR}/*.s)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGETNAME}.s.i
		COMMAND ${C_PREPROCESSOR} -traditional -nostdinc -undef -P -o ${CMAKE_CURRENT_BINARY_DIR}/${TARGETNAME}.s.i -I${LIB_PATH} ${CMAKE_CURRENT_LIST_DIR}/${SOURCEFILE}
		DEPENDS ${${TARGETNAME}_FILES} ${LIB_DEPS}
		COMMENT Preprocess ${SOURCEFILE}
	)

	# Assemble

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGETNAME}.bin
		COMMAND $<TARGET_FILE:mcasm> ${CMAKE_CURRENT_BINARY_DIR}/${TARGETNAME}.s.i ${CMAKE_CURRENT_BINARY_DIR}/${TARGETNAME}.bin
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${TARGETNAME}.s.i mcasm
		COMMENT Assemble ${SOURCEFILE}
	)

	# Create target
	add_custom_target(${TARGETNAME} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${TARGETNAME}.bin)
endmacro()

# Add programs
add_subdirectory(cputest)
