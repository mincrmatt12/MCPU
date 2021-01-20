set(MCPU_FPGA_FAMILY artix7 CACHE STRING "xc7 subfamily to route for")
set(MCPU_FPGA_CHIP xc7a100tcsg324-1 CACHE STRING "xc7 chip to route for")
set(MCPU_FPGA_BOARD arty CACHE STRING "board")

# find conda & helpers
find_program(ANACONDA conda REQUIRED)
find_program(JQ jq REQUIRED)

# find programs
find_program(YOSYS yosys REQUIRED)
find_program(NEXTPNR nextpnr-xilinx REQUIRED)
find_program(XCFASM xcfasm REQUIRED)

# grab conda env root dir
execute_process(
	COMMAND sh -c "${ANACONDA} info --json | ${JQ} .active_prefix -r"
	OUTPUT_VARIABLE ANACONDA_ROOT
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Conda env root is ${ANACONDA_ROOT}")

if (NOT EXISTS ${ANACONDA_ROOT}/share/nextpnr-xilinx)
	message(FATAL_ERROR "Missing nextpnr-xilinx in conda env")
endif()

if (NOT EXISTS ${ANACONDA_ROOT}/share/symbiflow/prjxray-db)
	message(FATAL_ERROR "Missing nextpnr-xilinx in conda env")
endif()

file(GLOB_RECURSE core_verilog_srcs ${CMAKE_CURRENT_LIST_DIR}/src/*.v)
set(              core_xdc_file ${CMAKE_CURRENT_LIST_DIR}/xdc/${MCPU_FPGA_BOARD}.xdc)

message(STATUS ${core_verilog_srcs})

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
	message(STATUS "OpenOCD found, adding flash target")

	add_custom_target(flash
		COMMAND
			${OPENOCD} -f ${ANACONDA_ROOT}/share/openocd/scripts/board/digilent_${MCPU_FPGA_BOARD}.cfg -c "init\; pld load 0 ${CMAKE_CURRENT_BINARY_DIR}/core.bit\; exit"
		DEPENDS
			core
	)
endif()
