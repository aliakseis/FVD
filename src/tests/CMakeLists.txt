cmake_minimum_required(VERSION 2.8)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)


do_test(strategies "strategies/test-strategies.cpp" "strategies/test-strategies.h")
