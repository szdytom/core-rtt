cmake_minimum_required(VERSION 3.26)

include(FetchContent)

set(CORERTT_VERSION "0.1")

project(corertt VERSION ${CORERTT_VERSION} LANGUAGES CXX)

# msft_proxy
FetchContent_Declare(msft_proxy4
	GIT_REPOSITORY https://github.com/microsoft/proxy.git
	GIT_TAG 4.0.1
	GIT_SHALLOW ON
	GIT_PROGRESS ON
	EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(msft_proxy4)

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

add_executable(corertt
	src/main.cpp
	src/tui.cpp
	src/noise.cpp
	src/xoroshiro.cpp
	src/tilemap.cpp
	src/world.cpp
	src/entity.cpp
	src/runtime.cpp
	src/log.cpp
)

target_compile_features(corertt PRIVATE cxx_std_23)

target_include_directories(corertt PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(corertt PRIVATE
	msft_proxy4::proxy
	cpptrace::cpptrace
	argparse::argparse
	riscv
	ftxui::screen
	ftxui::dom
	ftxui::component
)
