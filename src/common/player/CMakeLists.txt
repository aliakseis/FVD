cmake_minimum_required(VERSION 2.8)

# Project name
project(Player)

find_package(portaudio CONFIG)
if(NOT PORTAUDIO_FOUND)
    find_path(PORTAUDIO_INCLUDE_DIR
          NAMES portaudio.h
          DOC "The PortAudio include directory"
    )
    find_library(PORTAUDIO_LIBRARY
             NAMES portaudio
             DOC "The PortAudio library"
    )
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(PortAudio
        REQUIRED_VARS PORTAUDIO_LIBRARY PORTAUDIO_INCLUDE_DIR
    )
    if(PORTAUDIO_FOUND)
        set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARY})
        set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})
    endif()
endif()

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${FFMPEG_INCLUDE_DIR}
)

add_definitions(-D__STDC_CONSTANT_MACROS)
#add_definitions(-DQT_NO_DEBUG_OUTPUT)

# Debug output

set(HEADERS_LOCAL_TOMOC
    displaythread.h
    ffmpegdecoder.h
    widgetdisplay.h
)

if(DEVELOPER_OPENGL)
set(HEADERS_LOCAL_TOMOC
    ${HEADERS_LOCAL_TOMOC}
    opengldisplay.h
)
endif(DEVELOPER_OPENGL)

set(HEADERS_LOCAL
        audioparsethread.h
        parsethread.h
	performancecheck.h
	fpicture.h
	fqueue.h
	timelimiterthread.h
    	threadcontrol.h
    	videodisplay.h
	videoframe.h
        videoparsethread.h
	videoplayer.h
	vqueue.h
	interlockedadd.h
)

set(SOURCES_LOCAL
    audioparsethread.cpp
    displaythread.cpp
    ffmpegdecoder.cpp
	parsethread.cpp
	threadcontrol.cpp
    timelimiterthread.cpp
    videodisplay.cpp
    videoplayer.cpp
    videoparsethread.cpp
	widgetdisplay.cpp
)

if(DEVELOPER_OPENGL)
set(SOURCES_LOCAL
    ${SOURCES_LOCAL}
    opengldisplay.cpp
)
endif(DEVELOPER_OPENGL)

QTX_WRAP_CPP(MOC_LOCAL ${HEADERS_LOCAL_TOMOC})


source_group(sources FILES
    ${HEADERS_LOCAL_TOMOC}
				${HEADERS_LOCAL} 
				${SOURCES_LOCAL}
)


source_group(".generated" FILES
	${MOC_FILES}
)


add_library(Player STATIC
	${SOURCES_LOCAL}
	${MOC_LOCAL}
	${HEADERS_LOCAL}
	${HEADERS_LOCAL_TOMOC}
)


# External libraries
if(WIN32)
    set(FFMPEG_LIBRARIES_STANDART 
				${FFMPEG_LIBRARIES_STANDART}
				wsock32.lib
				winmm.lib
    				imm32.lib
    )
endif()

if(DEVELOPER_OPENGL)
    set(FFMPEG_LIBRARIES_STANDART 
				${FFMPEG_LIBRARIES_STANDART}
				${QT_QTOPENGL_LIBRARY}
    )
    if(WIN32)
        set(FFMPEG_LIBRARIES_STANDART
                                ${FFMPEG_LIBRARIES_STANDART}
                                opengl32
        )
    elseif(APPLE)
        find_library(MACOSX_FRAMEWORK_OPENGL OpenGL)
        set(FFMPEG_LIBRARIES_STANDART
                                ${FFMPEG_LIBRARIES_STANDART}
                                ${MACOSX_FRAMEWORK_OPENGL}
        )
    elseif(UNIX)
        set(FFMPEG_LIBRARIES_STANDART
                                ${FFMPEG_LIBRARIES_STANDART}
                                GL
        )
    endif(WIN32)
endif(DEVELOPER_OPENGL)

if(APPLE)
	find_library(MACOSX_FRAMEWORK_COCOA Cocoa)
	find_library(MACOSX_FRAMEWORK_AUDIO_TOOLBOX AudioToolbox)
	find_library(MACOSX_FRAMEWORK_CORE_VIDEO CoreVideo)
	find_library(MACOSX_FRAMEWORK_VDADECODER VideoDecodeAcceleration)
	set(MACOSX_FRAMEWORK_LIBS
		${MACOSX_FRAMEWORK_COCOA}
		${MACOSX_FRAMEWORK_AUDIO_TOOLBOX}
		${MACOSX_FRAMEWORK_CORE_VIDEO}
		${MACOSX_FRAMEWORK_VDADECODER}
	)

	find_library(BZIP2_LIB bz2)
	get_filename_component(BZIP2_LIB ${BZIP2_LIB} REALPATH)
	set(BZIP2_LIB ${BZIP2_LIB} PARENT_SCOPE)
	# append lib into src cmake

	find_package(Zlib REQUIRED)

	set(FFMPEG_LIBRARIES_STANDART 
				${FFMPEG_LIBRARIES_STANDART}
         		${BZIP2_LIB}
         		${ZLIB_LIBRARIES}
	)
elseif(UNIX)
#	find_library(PULSE_LIB pulse-simple)
#	set(FFMPEG_LIBRARIES_STANDART
#				${FFMPEG_LIBRARIES_STANDART}
#				${PULSE_LIB}
#	)
endif(APPLE)

add_dependencies(Player
        Utilities
)

target_link_libraries(Player
     	${FFMPEG_LIBRARIES_STANDART}
	${MACOSX_FRAMEWORK_LIBS}
        Utilities
        portaudio
)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "common")
