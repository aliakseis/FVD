# Find PYTHON - JSON handling library for Qt
#
# This module defines
#  PYTHON_FOUND - whether the qsjon library was found
#  PYTHON_LIBRARIES - the PYTHON library
#  PYTHON_INCLUDE_DIR - the include path of the PYTHON library
#

# Win32 prebuild
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(PYTHON_PLATFORM x64)
	else(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(PYTHON_PLATFORM x32)
	endif(CMAKE_SIZEOF_VOID_P EQUAL 8)

	if(WIN32)
		set(PYTHON_INCLUDE_DIR 
		    ${CMAKE_SOURCE_DIR}/imports/Python/include CACHE PATH "path to Python sources" FORCE
		)
		
		set(PYTHON_LIBS_DIR 
		    ${CMAKE_SOURCE_DIR}/imports/Python/lib CACHE PATH "path to Python libraries" FORCE
		)
	
		if(MSVC10)
			set(PYTHON_COMPILER vc10)
		elseif(MSVC11)
			set(PYTHON_COMPILER vc11)
		else()
			message(ERROR "Unknown windows compiler - using python for vc10")
			set(PYTHON_COMPILER vc10)
		endif()

		set(PYTHON_PLATFORM_SUBDIR ${PYTHON_COMPILER}-${PYTHON_PLATFORM})
		set(PYTHON_LIBRARIES
			optimized ${PYTHON_LIBS_DIR}/${PYTHON_PLATFORM_SUBDIR}/python27.lib 
			debug ${PYTHON_LIBS_DIR}/${PYTHON_PLATFORM_SUBDIR}/python27_d.lib 
		)
	endif()

if (PYTHON_INCLUDE_DIR AND PYTHON_LIBRARIES)

  # Already in cache
  set (PYTHON_FOUND TRUE)

else (PYTHON_INCLUDE_DIR AND PYTHON_LIBRARIES)

  if (NOT WIN32 AND NOT APPLE)
    # use pkg-config to get the values of PYTHON_INCLUDE_DIRS
    # and PYTHON_LIBRARY_DIRS to add as hints to the find commands.
    include (FindPkgConfig)
    pkg_check_modules (PYTHON REQUIRED python>=0.5)
  endif (NOT WIN32 AND NOT APPLE)

  find_library (PYTHON_LIBRARIES_
    NAMES
    python2.7
    HINTS
    /opt/local/lib
    /usr/local/opt/python/Frameworks/Python.framework/Versions/2.7/lib
    PATHS
    ${PYTHON_LIBRARY_DIRS}
    ${LIB_INSTALL_DIR}
    ${KDE4_LIB_DIR}
  )
  set(PYTHON_LIBRARIES ${PYTHON_LIBRARIES_})

  find_path (PYTHON_INCLUDE_DIR_
    NAMES
    Python.h
    PATH_SUFFIXES
    python2.7
    HINTS
    /opt/local/include
    /usr/local/opt/python/Frameworks/Python.framework/Versions/2.7/include
    PATHS
    ${PYTHON_INCLUDE_DIRS}
    ${INCLUDE_INSTALL_DIR}
    ${KDE4_INCLUDE_DIR}
  )
  set(PYTHON_INCLUDE_DIR ${PYTHON_INCLUDE_DIR_})

  message(STATUS "Python lib founded: ${PYTHON_LIBRARIES_}")
  message(STATUS "Python include dir founded: ${PYTHON_INCLUDE_DIR_}")

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(PYTHON DEFAULT_MSG PYTHON_LIBRARIES PYTHON_INCLUDE_DIR)

endif (PYTHON_INCLUDE_DIR AND PYTHON_LIBRARIES)


	
macro(INSTALL_PYTHON)
if(WIN32)

	file(GLOB PYTHON_PYDS_RELEASE ${CMAKE_SOURCE_DIR}/imports/Python/dll/${PYTHON_PLATFORM_SUBDIR}/*.pyd   )
	file(GLOB PYTHON_PYDS_DEBUG ${CMAKE_SOURCE_DIR}/imports/Python/dll/${PYTHON_PLATFORM_SUBDIR}/*_d.pyd )
	if(PYTHON_PYDS_DEBUG)
		list(REMOVE_ITEM PYTHON_PYDS_RELEASE ${PYTHON_PYDS_DEBUG})
	endif(PYTHON_PYDS_DEBUG)

	foreach(buildconfig ${CMAKE_CONFIGURATION_TYPES})
		if(${buildconfig} STREQUAL "Debug")
			set(PYTHON_DLL ../imports/Python/dll/${PYTHON_PLATFORM_SUBDIR}/python27_d.dll)
			set(SQLIGHT_DLL ../imports/Python/dll/${PYTHON_PLATFORM_SUBDIR}/sqlite3_d.dll)
			set(PYTHON_PYDS ${PYTHON_PYDS_DEBUG})
		else()
			set(PYTHON_DLL ../imports/Python/dll/${PYTHON_PLATFORM_SUBDIR}/python27.dll)
			set(SQLIGHT_DLL ../imports/Python/dll/${PYTHON_PLATFORM_SUBDIR}/sqlite3.dll)
			set(PYTHON_PYDS ${PYTHON_PYDS_RELEASE})
		endif()

		install(
			FILES ${PYTHON_DLL} ${SQLIGHT_DLL}
			DESTINATION .
			CONFIGURATIONS ${buildconfig}
		)
		install(
			FILES ${PYTHON_PYDS}
			DESTINATION DLLs
			CONFIGURATIONS ${buildconfig}
		)
	endforeach(buildconfig ${CMAKE_CONFIGURATION_TYPES})

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND
			${CMAKE_COMMAND} -E copy \"${CMAKE_SOURCE_DIR}/imports/Python/dll/${PYTHON_PLATFORM_SUBDIR}/python27$<$<CONFIG:Debug>:_d>.dll\" $<TARGET_FILE_DIR:${PROJECT_NAME}>
		COMMAND
			${CMAKE_COMMAND} -E copy \"${CMAKE_SOURCE_DIR}/imports/Python/dll/${PYTHON_PLATFORM_SUBDIR}/sqlite3$<$<CONFIG:Debug>:_d>.dll\" $<TARGET_FILE_DIR:${PROJECT_NAME}>
# TODO: add copying pyds.
	)

endif(WIN32)
	
endmacro(INSTALL_PYTHON)