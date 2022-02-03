cmake_minimum_required (VERSION 2.6)

set (PROJECT Preferences)

set (HEADERS 
	preferences.h)

set (SOURCES 
	preferences.cpp
	main.cpp)

	
set (RESOURCE_PATH 	.)

set (RESOURCES 
	${RESOURCE_PATH}/me.qrc)
	
set(FIM_UI_FORMS
	../../gui/preferences.ui)

project (${PROJECT})

find_package (Qt4 REQUIRED)
include (${QT_USE_FILE})
QTX_WRAP_UI(FIM_UI_H ${FIM_UI_FORMS})
QTX_ADD_RESOURCES (QRC_SOURCES ${RESOURCES})
QTX_WRAP_CPP (MOC_SOURCES ${HEADERS})


if (MSVC)
	add_definitions (/W3)
elseif (CMAKE_COMPILER_IS_GNUCXX)
	add_definitions (-Wall -pedantic)
else ()
	message ("Unknown compiler")
endif ()

source_group ("Header Files" FILES ${HEADERS})
source_group ("Source Files" FILES ${SOURCES})
source_group ("Generated Files" FILES ${MOC_SOURCES} ${FIM_UI_H})
source_group ("Resource Files" FILES ${QRC_SOURCES})
source_group(Forms FILES ${FIM_UI_FORMS})


include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(
	${PROJECT} 
	${HEADERS} 
	${SOURCES} 
	${MOC_SOURCES} 
	${FIM_UI_H}
	${QRC_SOURCES})

target_link_libraries( ${PROJECT} ${QT_LIBRARIES})
