cmake_minimum_required(VERSION 3.26)

include(FetchContent)

set(CORERTT_VERSION "0.1")

project(corertt VERSION ${CORERTT_VERSION} LANGUAGES CXX)

find_package(Git QUIET)
set(CORERTT_GIT_COMMIT_HASH "unknown")
if (Git_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE CORERTT_GIT_COMMIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		RESULT_VARIABLE CORERTT_GIT_HASH_RESULT
	)
	if (NOT CORERTT_GIT_HASH_RESULT EQUAL 0 OR CORERTT_GIT_COMMIT_HASH STREQUAL "")
		set(CORERTT_GIT_COMMIT_HASH "unknown")
	endif ()
endif ()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/include/corertt)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/include/corertt/build_config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/generated/include/corertt/build_config.h
	@ONLY
)

set(ZSTD_BUILD_STATIC ON)
set(ZSTD_BUILD_SHARED OFF)
set(ZSTD_BUILD_PROGRAMS OFF)
FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG        v1.5.7
    GIT_SHALLOW    ON
	SOURCE_SUBDIR build/cmake
)
FetchContent_MakeAvailable(zstd)

# cpptrace
FetchContent_Declare(
	cpptrace
	GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
	GIT_TAG        v1.0.4
	GIT_SHALLOW ON
	GIT_PROGRESS ON
	EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(cpptrace)

# argparse
FetchContent_Declare(
	argparse
	GIT_REPOSITORY https://github.com/p-ranav/argparse.git
	GIT_TAG v3.2
	GIT_SHALLOW ON
	GIT_PROGRESS ON
	EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(argparse)

FetchContent_Declare(
	riscv
	GIT_REPOSITORY https://github.com/libriscv/libriscv.git
	GIT_TAG v1.15
	GIT_SHALLOW ON
	GIT_PROGRESS ON
	EXCLUDE_FROM_ALL SYSTEM
)
option(RISCV_64I "" OFF)
option(RISCV_BINARY_TRANSLATION "" OFF)
option(RISCV_LIBTCC "" OFF)
option(RISCV_32I "" ON)
FetchContent_MakeAvailable(riscv)
target_compile_definitions(riscv PUBLIC RISCV_SYSCALLS_MAX=200)

FetchContent_Declare(
	ftxui
	GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
	GIT_TAG v6.1.9
	GIT_SHALLOW ON
	GIT_PROGRESS ON
	EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(ftxui)

add_library(corertt_core STATIC
	src/buffer.cpp
	src/stream_adapter.cpp
	src/noise.cpp
	src/xoroshiro.cpp
	src/tile_codec.cpp
	src/tilemap.cpp
	src/world.cpp
	src/entity.cpp
	src/runtime.cpp
	src/replay.cpp
)

target_compile_features(corertt_core PUBLIC cxx_std_23)

target_include_directories(corertt_core PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_BINARY_DIR}/generated/include
)

target_link_libraries(corertt_core PUBLIC
	cpptrace::cpptrace
	riscv
)

add_executable(corertt
	src/main.cpp
	src/cli_common.cpp
	src/plain_ui.cpp
	src/tui.cpp
)

target_include_directories(
	corertt PRIVATE
	${zstd_SOURCE_DIR}/lib
)

target_link_libraries(corertt PRIVATE
	corertt_core
	argparse::argparse
	cpptrace::cpptrace
	ftxui::screen
	ftxui::dom
	ftxui::component
	libzstd_static
)

add_executable(corertt_headless
	src/headless_main.cpp
	src/cli_common.cpp
)

target_include_directories(
	corertt_headless PRIVATE
	${zstd_SOURCE_DIR}/lib
)

target_link_libraries(corertt_headless PRIVATE
	corertt_core
	argparse::argparse
	cpptrace::cpptrace
	libzstd_static
)
