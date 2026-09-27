cmake_minimum_required(VERSION 3.5)

project(Download)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${FFMPEG_INCLUDE_DIR}
)

add_definitions(
	-DNOMINMAX 
	-D_CRT_SECURE_NO_WARNINGS 
	-D_CRT_NONSTDC_NO_WARNINGS 
	-DSILENT
)

set(HEADERS_DETAIL
	detail/downloader_base.h
	detail/downloader_signal_catchers.h
)

source_group("sources\\detail" FILES
	${HEADERS_TOMOC_DETAIL}
	${HEADERS_DETAIL}
)

set(HEADERS
	idownloader.h
	downloader.h
	downloader_traits.h
	FFmpegMergeDownloader.h
)

set(SOURCES
	detail/downloader_signal_catchers.cpp
	FFmpegMergeDownloader.cpp
)

source_group(sources FILES
	${HEADERS}
)

QTX_WRAP_CPP(MOC_FILES
	${HEADERS_TOMOC_DETAIL} 
)

source_group(".generated" FILES
	${MOC_FILES} 
)


add_library(Download STATIC
	${HEADERS_DETAIL}
	${HEADERS}
	${SOURCES}
)	

set(PROJECT_LIBRARIES Utilities)

if(DEVELOPER_TRAFFIC_CONTROL)
	set(PROJECT_LIBRARIES ${PROJECT_LIBRARIES} Traffic_Limitation)
endif(DEVELOPER_TRAFFIC_CONTROL)

add_dependencies(Download ${PROJECT_LIBRARIES})

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "common")
