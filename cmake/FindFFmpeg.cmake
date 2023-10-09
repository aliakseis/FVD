# Find FFmpeg script

set(FFMPEG_INLUDE_SEARCHHINTS "")
set(FFMPEG_LIBRARIES_SEARCHHINTS "")

set(FFMPEG_INLUDE_SEARCHPATHS
	"/usr/include"
	"/usr/local/include"
	"/opt/local/include"
	"/sw/include"
	"/usr/include/ffmpeg"
	"/usr/local/include/ffmpeg"
	"/opt/local/include/ffmpeg"
	"/sw/include/ffmpeg"
)
	
set(FFMPEG_LIBRARIES_SEARCHPATHS
	"/usr/lib"
	"/usr/local/lib"
	"/opt/local/lib"
	"/sw/lib"
)


if(APPLE)
	set(FFMPEG_INLUDE_SEARCHHINTS
					"${CMAKE_SOURCE_DIR}/imports/ffmpeg-mac/include"
					${FFMPEG_INLUDE_SEARCHHINTS}
	)
	set(FFMPEG_LIBRARIES_SEARCHHINTS
					"${CMAKE_SOURCE_DIR}/imports/ffmpeg-mac/lib"
					${FFMPEG_LIBRARIES_SEARCHHINTS}
	)
endif(APPLE)

# Find inlude path
find_path(FFMPEG_INCLUDE_DIR
		NAMES
			libavcodec/avcodec.h
		HINTS
			${FFMPEG_INLUDE_SEARCHHINTS}
		PATHS
			${FFMPEG_INLUDE_SEARCHPATHS}
		PATH_SUFFIXES
			libavcodec
)



find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)

find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)
find_library(SWSCALE_LIBRARY swscale)

find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h)
find_library(SWRESAMPLE_LIBRARY swresample)

set(FFMPEG_LIBRARIES 
		${AVCODEC_LIBRARY}
		${AVDEVICE_LIBRARY}
		${AVFILTER_LIBRARY}
		${AVFORMAT_LIBRARY}
		${AVRESAMPLE_LIBRARY}
		${AVUTIL_LIBRARY}
		${POSTPROC_LIBRARY}
		${SWRESAMPLE_LIBRARY}
		${SWSCALE_LIBRARY}
)

set(FFMPEG_LIBRARIES_STANDART
		${AVUTIL_LIBRARY}
		${AVCODEC_LIBRARY}
		${AVFORMAT_LIBRARY}
		${SWRESAMPLE_LIBRARY}
		${SWSCALE_LIBRARY}
)

if(FFMPEG_INCLUDE_DIR AND AVCODEC_LIBRARY)
	set(FFMPEG_FOUND TRUE)
	message( STATUS "Found FFmpeg libraries: ${FFMPEG_LIBRARIES}" )
	message( STATUS "Found FFmpeg include directory: ${FFMPEG_INCLUDE_DIR}" )

	# Try to find path with dynamic dlls
	if(WIN32)
		if(EXISTS "${FFMPEG_INCLUDE_DIR}/../bin")
			get_filename_component(FFMPEG_BINARY_DIR "${FFMPEG_INCLUDE_DIR}/../bin" ABSOLUTE)
			message( STATUS "Found FFmpeg dynamic binaries directory: ${FFMPEG_BINARY_DIR}" )
		endif(EXISTS "${FFMPEG_INCLUDE_DIR}/../bin")
	endif(WIN32)

	# c99 FIX
	if(WIN32)
		set(FFMPEG_INCLUDE_DIR ${FFMPEG_INCLUDE_DIR} ${FFMPEG_C99_WINDIR})
	endif(WIN32)
else(FFMPEG_INCLUDE_DIR AND AVCODEC_LIBRARY)
	message( STATUS "Found FFmpeg libraries: ${FFMPEG_LIBRARIES}" )
	message( STATUS "Found FFmpeg include directory: ${FFMPEG_INCLUDE_DIR}" )
	message( FATAL_ERROR "FFmpeg NOT FOUND")
endif(FFMPEG_INCLUDE_DIR AND AVCODEC_LIBRARY)


macro(INSTALL_FFMPEG_DLLS)
	if(WIN32)

		FILE(GLOB FFMPEG_DLLS_BASIC
			${FFMPEG_BINARY_DIR}/avcodec-*.dll
			${FFMPEG_BINARY_DIR}/avformat-*.dll
			${FFMPEG_BINARY_DIR}/avutil-*.dll
			${FFMPEG_BINARY_DIR}/swscale-*.dll
			${FFMPEG_BINARY_DIR}/swresample-*.dll
		)

		install(FILES
				${FFMPEG_DLLS_BASIC}
				DESTINATION .
				COMPONENT Runtime
		)

		#foreach(COPY_FF_LIB ${FFMPEG_DLLS_BASIC})
		#	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy \"${COPY_FF_LIB}\" ${BINARY_PATH}/)
		#endforeach(COPY_FF_LIB ${FFMPEG_DLLS_BASIC})
	endif(WIN32)
endmacro(INSTALL_FFMPEG_DLLS)
