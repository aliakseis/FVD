
project(tinyhttpserver)




file(GLOB SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

QT4_WRAP_CPP(MOC_CPP ${HDRS})

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
#	${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty
	${CMAKE_CURRENT_BINARY_DIR}
)

source_group("Generated" FILES ${MOC_CPP})

add_executable(tinyhttpserver ${SRCS} ${HDRS} ${MOC_CPP})

INSTALL_QT(tinyhttpserver QtCore QtGui QtMain QtXml)

target_link_libraries(tinyhttpserver tiny http server)
add_dependencies(tinyhttpserver tiny http server)
