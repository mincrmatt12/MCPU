set(MCPU_FPGA_FAMILY artix7 CACHE STRING "xc7 subfamily to route for")
set(MCPU_FPGA_CHIP xc7a100tcsg324-1 CACHE STRING "xc7 chip to route for")
set(MCPU_FPGA_BOARD arty CACHE STRING "board")

# find conda & helpers
find_program(ANACONDA conda)
find_program(JQ jq)

# find programs
find_program(YOSYS yosys)
if (NOT YOSYS)
	message(FATAL_ERROR "Missing yosys")
endif()
find_program(NEXTPNR nextpnr-xilinx)
if (NOT NEXTPNR)
	message(FATAL_ERROR "Missing nextpnr-xilinx")
endif()
find_program(XCFASM xcfasm)
if (NOT XCFASM)
	message(FATAL_ERROR "Missing xcfasm")
endif()

# grab conda env root dir

if (NOT EXISTS ${ANACONDA_ROOT})
	if (NOT ANACONDA OR NOT JQ)
		message(FATAL_ERROR "Anaconda root not set and we can't find conda or jq")
	endif()
	message(STATUS "Anaconda root not set properly, checking current environment...")

	execute_process(
		COMMAND sh -c "${ANACONDA} info --json | ${JQ} .active_prefix -r"
		OUTPUT_VARIABLE ANACONDA_ROOT_
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	set(ANACONDA_ROOT ${ANACONDA_ROOT_} CACHE PATH "root of conda env" FORCE)
endif()

message(STATUS "Conda env root is ${ANACONDA_ROOT}")

if (NOT EXISTS ${ANACONDA_ROOT}/share/nextpnr-xilinx)
	message(FATAL_ERROR "Missing nextpnr-xilinx in conda env")
endif()

if (NOT EXISTS ${ANACONDA_ROOT}/share/symbiflow/prjxray-db)
	message(FATAL_ERROR "Missing nextpnr-xilinx in conda env")
endif()

file(GLOB_RECURSE core_verilog_srcs ${CMAKE_CURRENT_LIST_DIR}/src/*.v)
set(              core_xdc_file ${CMAKE_CURRENT_LIST_DIR}/xdc/${MCPU_FPGA_BOARD}.xdc)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/core.json
	COMMAND
		${YOSYS} -p "synth_xilinx -flatten -abc9 -arch xc7 -top top\; write_json ${CMAKE_CURRENT_BINARY_DIR}/core.json" ${core_verilog_srcs}
	DEPENDS
		${core_verilog_srcs}
	COMMENT
		"Synthesizing FPGA design"
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/core.fasm
	COMMAND
		${NEXTPNR} --chipdb ${ANACONDA_ROOT}/share/nextpnr-xilinx/${MCPU_FPGA_CHIP}.bin --xdc ${core_xdc_file} --json ${CMAKE_CURRENT_BINARY_DIR}/core.json --write ${CMAKE_CURRENT_BINARY_DIR}/core.routed.json --fasm ${CMAKE_CURRENT_BINARY_DIR}/core.fasm
	DEPENDS
		${CMAKE_CURRENT_BINARY_DIR}/core.json
		${core_xdc_file}
	BYPRODUCTS
		${CMAKE_CURRENT_BINARY_DIR}/core.routed.json
	COMMENT
		"Routing FPGA design"
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/core.bit
	COMMAND
		${XCFASM} --part ${MCPU_FPGA_CHIP} --db-root ${ANACONDA_ROOT}/share/symbiflow/prjxray-db/${MCPU_FPGA_FAMILY} --fn_in ${CMAKE_CURRENT_BINARY_DIR}/core.fasm --part_file ${ANACONDA_ROOT}/share/symbiflow/prjxray-db/${MCPU_FPGA_FAMILY}/${MCPU_FPGA_CHIP}/part.yaml --bit_out ${CMAKE_CURRENT_BINARY_DIR}/core.bit
	DEPENDS
		${CMAKE_CURRENT_BINARY_DIR}/core.fasm
	COMMENT
		"Assembling FPGA bitstream"
)

add_custom_target(core ALL
	DEPENDS
		${CMAKE_CURRENT_BINARY_DIR}/core.bit
)

find_program(OPENOCD openocd)

if (OPENOCD)
	message(STATUS "OpenOCD found, adding upload target")

	add_custom_target(upload
		COMMAND
			${OPENOCD} -f ${ANACONDA_ROOT}/share/openocd/scripts/board/digilent_${MCPU_FPGA_BOARD}.cfg -c "init\; pld load 0 ${CMAKE_CURRENT_BINARY_DIR}/core.bit\; exit"
		DEPENDS
			core
	)
endif()
