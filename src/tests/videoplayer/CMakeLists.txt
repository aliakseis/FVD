cmake_minimum_required(VERSION 2.8)
project(ffmpegdisplay)

#Compiling flags
add_definitions(-DQT_STATICPLUGINS 
		-DRARDLL 
		-DNOMINMAX 
		-D_CRT_SECURE_NO_WARNINGS 
		-D_CRT_NONSTDC_NO_WARNINGS 
		-DSILENT
		)

		if(MSVC)
			set(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /MDd /Zi /Ob0 /Od /RTC1")
			set(CMAKE_C_FLAGS_MINSIZEREL     "/MD /O1 /Ob1 /D NDEBUG")
			set(CMAKE_C_FLAGS_RELEASE        "/MD /O2 /Ob2 /D NDEBUG")
			set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob1 /D NDEBUG")

			set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MDd /Zi /Ob0 /Od /RTC1" )
			set(CMAKE_CXX_FLAGS_MINSIZEREL     "/MD /O1 /Ob1 /D NDEBUG")
			set(CMAKE_CXX_FLAGS_RELEASE        "/MD /O2 /Ob2 /D NDEBUG")
			set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob1 /D NDEBUG")
		endif()

find_package(Qt4 REQUIRED)
set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../imports/ffmpeg")

include_directories(
		    ${CMAKE_CURRENT_BINARY_DIR}
		    ${CMAKE_CURRENT_SOURCE_DIR}
		    "${CMAKE_CURRENT_SOURCE_DIR}/.."
		    "${FFMPEG_DIR}/include"
		    ${QT_INCLUDES}
		    "${FFMPEG_DIR}/c99/os-win32"
		    "${FFMPEG_DIR}/c99/wtf"
		    "${FFMPEG_DIR}/c99"
		    )
add_definitions(-D__STDC_CONSTANT_MACROS ${QT_DEFINITIONS})

QTX_WRAP_CPP(MOCFILES mainwindow.h ../ffmpegdecoder.h ../displaythread.h ../threadcontrol.h ../opengldisplay.h ../widgetdisplay.h ../videodisplay.h)
QTX_WRAP_UI(UIFILES mainwindow.ui)

add_executable(ffmpegdisplay main.cpp ../ffmpegdecoder.cpp ../displaythread.cpp ../threadcontrol.cpp ../opengldisplay.cpp ../widgetdisplay.cpp ../videodisplay.cpp mainwindow.cpp ${MOCFILES} ${UIFILES})
target_link_libraries(ffmpegdisplay 
				     "${FFMPEG_DIR}/lib/avcodec.lib"
				     "${FFMPEG_DIR}/lib/avformat.lib"
				     "${FFMPEG_DIR}/lib/avutil.lib"
				     "${FFMPEG_DIR}/lib/swscale.lib"
				     wsock32.lib
				     winmm.lib
                     imm32.lib
				    ${QT_QTCORE_LIBRARY}
				    ${QT_QTGUI_LIBRARY}
					${QT_QTOPENGL_LIBRARY}
					opengl32
		     )

install (FILES "${FFMPEG_DIR}/bin/avcodec-54.dll" DESTINATION .)
install (FILES "${FFMPEG_DIR}/bin/avformat-54.dll" DESTINATION .)
install (FILES "${FFMPEG_DIR}/bin/avutil-51.dll" DESTINATION .)
install (FILES "${FFMPEG_DIR}/bin/swscale-2.dll" DESTINATION .)

get_target_property(BINARY_FILE ffmpegdisplay LOCATION)
get_filename_component(BINARY_PATH ${BINARY_FILE} PATH)

add_custom_command(TARGET ffmpegdisplay POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy \"${FFMPEG_DIR}/bin/avcodec-54.dll\" ${BINARY_PATH}/)
add_custom_command(TARGET ffmpegdisplay POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy \"${FFMPEG_DIR}/bin/avformat-54.dll\" ${BINARY_PATH}/)
add_custom_command(TARGET ffmpegdisplay POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy \"${FFMPEG_DIR}/bin/avutil-51.dll\" ${BINARY_PATH}/)
add_custom_command(TARGET ffmpegdisplay POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy \"${FFMPEG_DIR}/bin/swscale-2.dll\" ${BINARY_PATH}/)
