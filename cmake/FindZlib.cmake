# Find ZLIB
#
# This module defines
#  ZLIB_FOUND - whether the zlib library was found
#  ZLIB_LIBRARIES - the zlib library
#  ZLIB_INCLUDE_DIR - the include path of the zlib library
#

# Win32 prebuild
if(WIN32)
	find_package(Qt4 REQUIRED)
	set(QT_3RDPARTY_DIR
		${QT_BINARY_DIR}/../src/3rdparty
	)
	set(ZLIB_INCLUDE_DIR ${QT_3RDPARTY_DIR}/zlib)
	set(ZLIB_LIBRARIES zlib)
endif(WIN32)


if (ZLIB_INCLUDE_DIR AND ZLIB_LIBRARIES)

  # Already in cache
  set (ZLIB_FOUND TRUE)

else (ZLIB_INCLUDE_DIR AND ZLIB_LIBRARIES)

  find_library (ZLIB_LIBRARIES
    NAMES
    z
    HINTS
    /opt/local/lib
    PATHS
    ${ZLIB_LIBRARY_DIRS}
    ${LIB_INSTALL_DIR}
    ${KDE4_LIB_DIR}
  )

  find_path (ZLIB_INCLUDE_DIR
    NAMES
    zlib.h
    HINTS
    /opt/local/include
    PATHS
    ${ZLIB_INCLUDE_DIRS}
    ${INCLUDE_INSTALL_DIR}
    ${KDE4_INCLUDE_DIR}
  )

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(ZLIB DEFAULT_MSG ZLIB_LIBRARIES ZLIB_INCLUDE_DIR)

endif (ZLIB_INCLUDE_DIR AND ZLIB_LIBRARIES)
