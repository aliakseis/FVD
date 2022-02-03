project (Lua)
cmake_minimum_required (VERSION 2.8)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

aux_source_directory(${PROJECT_SOURCE_DIR} lua_srcs)
if (WIN32)
  add_definitions( -DWIN32 -D_WIN32)
endif (WIN32)

add_library (lua STATIC ${lua_srcs})