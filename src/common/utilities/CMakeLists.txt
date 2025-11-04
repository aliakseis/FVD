cmake_minimum_required(VERSION 3.5)

project(Utilities)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(
	-DNOMINMAX 
	-D_CRT_SECURE_NO_WARNINGS 
	-D_CRT_NONSTDC_NO_WARNINGS 
	-DSILENT
)


set(HEADERS_TOMOC
	authentication_helper.h
	event_timer.h
	notify_helper.h
)

set(HEADERS
	authentication_helper.h
	credential.h
	credsretriever.h
	errorcode.h
	logger.h
	loggertag.h
	qttranslation.h
	singleton.h
	translation.h
	translatable.h
	utils.h
	notify_helper.h
	modelserializer.h
	modeldeserializer.h
	modelserializecommon.h
	iparentadvice.h
	instantiator.h
	patricia.h
	customutf8codec.h
	windowsfirewall.h
	filesaveguard.h
	filesystem_utils.h
)

set(SOURCES
	authentication_helper.cpp
	errorcode.cpp
	event_timer.cpp
	logger.cpp
	loggertag.cpp
	translation.cpp
	translatable.cpp
	utils.cpp
	modelserializer.cpp
	modeldeserializer.cpp
	modelserializecommon.cpp
	customutf8codec.cpp
	windowsfirewall.cpp
	filesystem_utils.cpp
)


source_group(sources FILES
	${HEADERS_TOMOC}
	${HEADERS}
	${SOURCES}
)


QTX_WRAP_CPP(MOC_FILES
	${HEADERS_TOMOC}
)

source_group(".generated" FILES
	${MOC_FILES}
)

set(FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE}
	${CMAKE_CURRENT_SOURCE_DIR}/qttranslation.h 
	${CMAKE_CURRENT_SOURCE_DIR}/errorcode.h
	${CMAKE_CURRENT_SOURCE_DIR}/errorcode.cpp
	PARENT_SCOPE
)

add_library(Utilities STATIC
	${HEADERS_TOMOC}
	${HEADERS}
	${SOURCES}
	${MOC_FILES}
)	

if(APPLE)
	add_dependencies(Utilities 
		Darwin
	)
endif(APPLE)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "common")
