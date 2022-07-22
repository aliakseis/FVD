cmake_minimum_required(VERSION 3.0)

if(POLICY CMP0020)
	cmake_policy(SET CMP0020 NEW)
endif(POLICY CMP0020)

if(POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif(POLICY CMP0048)

###################################################
### SECTION Project Settings
###################################################

set(PROJECT_NAME
	"FVD" CACHE STRING "Project Name aka brand name"
)

set(PROJECT_FULLNAME
	"Free Video Downloader" CACHE STRING "${PROJECT_NAME} fullname"
)

set(PROJECT_DOMAIN
	"github.com" CACHE STRING "Internet domain name for ${PROJECT_NAME}"
)

set(PROJECT_COMPANYNAME
	"aliakseis" CACHE STRING "Company name"
)

set(CMAKE_MODULE_PATH 
	${CMAKE_MODULE_PATH} 
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

project(${PROJECT_NAME} VERSION 0.0.0.5)
message("Building ${PROJECT_NAME} version ${PROJECT_VERSION}")

###################################################
### END OF SECTION Project Settings
###################################################


###################################################
### SECTION Options
###################################################

option(DEVELOPER_BUILD_TESTS "Use Qt unit tests" OFF)
option(DEVELOPER_FEATURES "use some debug tricks for developers" OFF)
option(DEVELOPER_GENERATE_TRANSLATIONS "Generate ts-files for ${PROJECT_NAME}" OFF)

option(DEVELOPER_TRAFFIC_CONTROL "allow traffic control functionality" ON)
option(DEVELOPER_OPENGL "Enables opengl display" ON)

option (LUA_SCRIPTING "Enables lua script engine" OFF)


###################################################
### END OF SECTION Options
###################################################


###################################################
###  SECTION Base build settings
###################################################

# Using pre-defined macros for this project
include(cmake/projecthelper.cmake)
include(cmake/testing.cmake)
include(cmake/doxygenHelper.cmake)
include(cmake/machelper.cmake)
include(cmake/additionalOptions.cmake)
include(cmake/VersionConf.cmake)
include(cmake/integrate-qt.cmake)


if(WIN32 OR APPLE)
	set(CMAKE_INSTALL_PREFIX 
		"${CMAKE_BINARY_DIR}/bin"
	)
endif(WIN32 OR APPLE)

if (APPLE)
	set (CLIENT_DATA_DIR "${PROJECT_NAME}.app")
	set(BINARY_INSTALL_DIR .)
elseif (WIN32)
	set (CLIENT_DATA_DIR ".")
	set(BINARY_INSTALL_DIR .)
else ()
	set (CLIENT_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}")
	set(BINARY_INSTALL_DIR bin)
endif()

# Setup testing utility for project
SETUP_TESTING()

###################################################
### END OF SECTION Base build settings
###################################################


###################################################
###  SECTION Macro Definitions
###################################################

DEFINE_DEFAULT_DEFINITIONS()


if(DEVELOPER_TRAFFIC_CONTROL)
	add_definitions(
		-DALLOW_TRAFFIC_CONTROL
	)
endif(DEVELOPER_TRAFFIC_CONTROL)

if(DEVELOPER_OPENGL)
	add_definitions(
		-DDEVELOPER_OPENGL
	)
endif(DEVELOPER_OPENGL)


###################################################
###  END OF SECTION Macro Definitions
###################################################


###################################################
###  SECTION Frameworks and Libraries
###################################################

######
# Qt
######


set(QT_COMPONENTS_TO_USE 
	Qt5Core
	Qt5Concurrent
	Qt5Gui
	Qt5Widgets
	Qt5Network
	Qt5QuickWidgets
	Qt5LinguistTools	# ?
	Qt5Qml
	Qt5Quick
	Qt5QuickControls2
        #Qt5QuickTemplates2
	)


INTEGRATE_QT(${QT_COMPONENTS_TO_USE})


###########
# FFMPEG
###########

find_package (FFmpeg REQUIRED)

###########
# OpenSSL
###########

find_package(OpenSSL REQUIRED)


###########
# Zlib
###########


###########
# Lua?
###########

if(LUA_SCRIPTING)
	find_package(Lua51 QUIET)
	if(NOT LUA_FOUND AND WIN32)
		add_subdirectory(src/3rdparty/lua)
		include_directories ("${CMAKE_SOURCE_DIR}/src/3rdparty/lua")
		set (LUA_LIBRARIES "lua")
	else()
		include_directories(${LUA_INCLUDE_DIR})
	endif()
	add_definitions(-DWITH_LUA)
endif(LUA_SCRIPTING)

###################
# Doxygen
###################

SETUP_DOXYGEN()

###################
# Using externals
###################

include_directories(
	${OPENSSL_INCLUDE_DIR}
	${FFMPEG_INCLUDE_DIR}
	${FFMPEG_INCLUDE_C99_DIR}
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_BINARY_DIR}/src
)

set(EXTERNAL_LIBS
	${QT_QGIF_LIBRARY} 
	${QT_QICO_LIBRARY}	
	${FFMPEG_LIBRARIES_STANDART}
	${OPENSSL_LIBRARIES}
	${IMAGE_LIBS}
)

if(APPLE)
	find_library(MACOSX_FRAMEWORK_COCOA Cocoa)
	find_library(MACOSX_FRAMEWORK_QTKIT QTKit)
	find_library(MACOSX_FRAMEWORK_APPKIT AppKit)
	
	set(MACOSX_FRAMEWORK_LIBS
		"-lz"
		${MACOSX_FRAMEWORK_COCOA}
		${MACOSX_FRAMEWORK_QTKIT}
	)

	set(EXTERNAL_LIBS
		${EXTERNAL_LIBS}
		${MACOSX_FRAMEWORK_LIBS}
	)
endif(APPLE)

###################################################
###  END OF SECTION Frameworks and Libraries
###################################################


###################################################
###  SECTION Resources
###################################################

################
# Qt resources
################

set(RESOURCE_QT
	${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
)

# resolve unix imageprefix issue
if(WIN32 OR APPLE)
	set(RESOURCE_QT
		${RESOURCE_QT}
		${CMAKE_CURRENT_SOURCE_DIR}/resources/localformats.qrc
	)
endif()

################
# OS resources
################

if(WIN32)
	set(RESOURCE_OS    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/winres.rc")
	set(ICON_FILE        "${CMAKE_SOURCE_DIR}/resources/images/downloader.ico")
	set(PLAY_ICON_FILE        "${CMAKE_SOURCE_DIR}/resources/images/play.ico")
	set(PAUSE_ICON_FILE        "${CMAKE_SOURCE_DIR}/resources/images/pause.ico")
	set(RESOURCE_OS_IN "${CMAKE_CURRENT_SOURCE_DIR}/resources/winres.rc.in")
#	VersionConf(${PROJECT_NAME} ${RESOURCE_OS_IN} ${RESOURCE_OS}) -- moved after the target generated
#	configure_file(${RESOURCE_OS_IN} ${RESOURCE_OS} @ONLY)
elseif(APPLE)
	set(MACOSX_BUNDLE_ICON_FILE 
		downloader.icns
	)
	set(MACOSX_APPICON 
		${CMAKE_CURRENT_SOURCE_DIR}/resources/images/${MACOSX_BUNDLE_ICON_FILE}
	)
	
	#Stub. rewrite on plist required
	set(RESOURCE_OS
			${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/fvdownloader.desktop
	)
	set(RESOURCE_OS_IN
			${CMAKE_CURRENT_SOURCE_DIR}/resources/fvdownloader.desktop.in
	)
	set(ICON_FILE
		${MACOSX_APPICON}
	)
elseif(UNIX)
	set(RESOURCE_OS
			${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/fvdownloader.desktop
	)
	set(RESOURCE_OS_IN
			${CMAKE_CURRENT_SOURCE_DIR}/resources/fvdownloader.desktop.in
	)
endif()


################
# QML resources
################



	add_definitions(
		-DDEVELOPER_QUICK2
	)
	set(RESOURCE_QML ${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2.qrc)
	set(QML_FILES
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/components/BusyIndicator.qml
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/components/Button.qml
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/components/LabelWithTooltip.qml
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/components/FramedImage.qml
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/components/TextLine.qml
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/components/Tooltip.qml
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/components/TooltipLoader.qml
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/LibraryItemDelegate.qml
		${CMAKE_CURRENT_SOURCE_DIR}/resources/qml2/LibraryView.qml
	)


###############
# JS resources
###############

set(JS_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/resources/strategies/YouTube.js
	${CMAKE_CURRENT_SOURCE_DIR}/resources/strategies/Vimeo.js 
	${CMAKE_CURRENT_SOURCE_DIR}/resources/strategies/Metacafe.js 
	${CMAKE_CURRENT_SOURCE_DIR}/resources/strategies/Dailymotion.js 
)

###############
# Python resources
###############

set(PY_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/resources/strategies/YouTube.py
)

##########
# Styles
##########

set(CSS_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/resources/style.css
)

###################################################
###  END OF SECTION Resources
###################################################



###################################################
###  SECTION Languages Definitions
###################################################

set(TS_FILES
	${CMAKE_SOURCE_DIR}/translations/translations_en.ts
	${CMAKE_SOURCE_DIR}/translations/translations_fr.ts
	${CMAKE_SOURCE_DIR}/translations/translations_es.ts
	${CMAKE_SOURCE_DIR}/translations/translations_pt.ts
	${CMAKE_SOURCE_DIR}/translations/translations_tr.ts
	${CMAKE_SOURCE_DIR}/translations/translations_de.ts
	${CMAKE_SOURCE_DIR}/translations/translations_it.ts
	${CMAKE_SOURCE_DIR}/translations/translations_ru.ts
)


set(FILES_TO_TRANSLATE
	${CMAKE_SOURCE_DIR}/resources/qml2/LibraryItemDelegate.qml
)

include(cmake/install-translations.cmake)

###################################################
###  END OF SECTION Languages Definitions
###################################################


set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_subdirectory(src)
