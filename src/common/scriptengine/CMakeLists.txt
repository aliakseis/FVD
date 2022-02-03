cmake_minimum_required(VERSION 2.8)

# Project name
project(ScriptEngine)

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/pythonqt/src
	${CMAKE_CURRENT_BINARY_DIR}
	${Python3_INCLUDE_DIRS}
)

set(HEADERS_LOCAL_TOMOC
)

set(HEADERS_LOCAL
	luaengine.h
	jsengine.h
	scriptengine.h
	pythonengine.h
)

set(SOURCES_LOCAL
	luaengine.cpp
	jsengine.cpp
	pythonengine.cpp
)

set(LINK_LIBRARIES_LOCAL
        ""
)


#QT4_WRAP_CPP(MOC_LOCAL ${HEADERS_LOCAL_TOMOC})

source_group("Sources" FILES 
				${HEADERS_LOCAL} 
				${HEADERS_LOCAL_TOMOC}
				${SOURCES_LOCAL})


add_library(${PROJECT_NAME} STATIC
	${SOURCES_LOCAL}
	${MOC_LOCAL}
	${HEADERS_LOCAL}
	${HEADERS_LOCAL_TOMOC}
)

target_link_libraries(${PROJECT_NAME}
	${LINK_LIBRARIES_LOCAL}
	${LUA_LIBRARIES}
)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "common")
