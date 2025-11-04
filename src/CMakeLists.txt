cmake_minimum_required(VERSION 3.5)

###################################################
###  SECTION Paths
###################################################

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/main
	${CMAKE_CURRENT_SOURCE_DIR}/gui
	${CMAKE_CURRENT_SOURCE_DIR}/logic
	${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
	${CMAKE_CURRENT_SOURCE_DIR}/common
	${CMAKE_CURRENT_SOURCE_DIR}/scripting
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/main
	${CMAKE_CURRENT_BINARY_DIR}/gui
	${CMAKE_CURRENT_BINARY_DIR}/logic
	${CMAKE_CURRENT_BINARY_DIR}/3rdparty
	${CMAKE_CURRENT_BINARY_DIR}/common
	${CMAKE_CURRENT_BINARY_DIR}/scripting
)

###################################################
###  END OF SECTION Paths
###################################################


###################################################
###  SECTION Modules 
###################################################

find_package (Python3 COMPONENTS Interpreter Development REQUIRED)

message ("    *** Python: (${Python3_INCLUDE_DIRS})")

file(GLOB PYTHONQT_SRCS 
	./3rdparty/pythonqt/src/*.cpp 
	./3rdparty/pythonqt/generated_cpp_56/com_trolltech_qt_core_builtin/*.cpp
	./3rdparty/pythonqt/generated_cpp_56/com_trolltech_qt_gui_builtin/*.cpp
)
file(GLOB PYTHONQT_HDRS 
	./3rdparty/pythonqt/src/*.h
	./3rdparty/pythonqt/generated_cpp_56/com_trolltech_qt_core_builtin/*.h
	./3rdparty/pythonqt/generated_cpp_56/com_trolltech_qt_gui_builtin/*.h
)
QTX_WRAP_CPP(PYTHONQT_MOC_FILES ${PYTHONQT_HDRS})              
add_library(pythonqt SHARED
               ${PYTHONQT_SRCS} ${PYTHONQT_MOC_FILES})
target_include_directories(pythonqt PRIVATE ./3rdparty/pythonqt/src ${Python3_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS}/cpython ${Qt5Core_PRIVATE_INCLUDE_DIRS})
target_compile_definitions(pythonqt PUBLIC PYTHONQT_EXPORTS)
target_link_libraries(pythonqt
                      ${Python3_LIBRARIES} Qt5::Core Qt5::Gui Qt5::Widgets)


add_subdirectory(3rdparty/qtsingleapplication)

if(APPLE)
	add_subdirectory(common/darwin)
	set(DARWIN_LIB Darwin)
endif(APPLE)

add_subdirectory(common/utilities)
add_subdirectory(common/download)
add_subdirectory(common/player)
add_subdirectory(common/ui_utils)
add_subdirectory(common/scriptengine)

if(DEVELOPER_TRAFFIC_CONTROL)
	add_subdirectory(common/traffic_limitation)
endif(DEVELOPER_TRAFFIC_CONTROL)


# Add a custom target to run the shell script before building the project
add_custom_target(update_version_file
    COMMAND ${CMAKE_SOURCE_DIR}/src/main/update_version.sh
)

###################################################
###  END OF SECTION Modules 
###################################################


####################################################
###  SECTION Functional module setting
###################################################

set(FUNCTIONAL_MODULE
	"FVD_lib" CACHE STRING "Module with functional of the program"
)

###################################################
###  END OF SECTION Functional module setting
###################################################


####################################################
###  SECTION Sources
###################################################

##############
# Main group
##############

set(HEADERS_MAIN 
	main/application.h
	main/global_functions.h
	main/globals.h
	main/settings_declaration.h
)

set(SOURCES_MAIN
	main/global_functions.cpp
)

source_group(main FILES
	${HEADERS_MAIN}
	${SOURCES_MAIN}
#	${RESOURCE_OS}
)


##############
# GUI group
##############

set(HEADERS_TOMOC_GUI
        gui/barberpoleprogress.h
        gui/clickablelabel.h
	gui/ellipsislabel.h
	gui/customdockwidget.h
	gui/descriptionpanel.h
	gui/downloadscontrol.h
	gui/downloadsdelegate.h
	gui/downloadsform.h
	gui/faenzastyle.h
	gui/itemsdelegate.h
	gui/libraryform.h
	gui/maintoolbar.h
	gui/mainwindow.h
	gui/playerheader.h
	gui/preferences.h
	gui/searchresultform.h
	gui/sitescheckboxdelegate.h
	gui/sitescombo.h
	gui/spinner.h
	gui/videocontrol.h
	gui/videoplayerwidget.h
	gui/videoprogressbar.h
	gui/videoqualitydialog.h
	gui/videowidget.h

	gui/mousehoverbutton.h
	gui/previewpanelbutton.h
	gui/volumeprogressbar.h
)

set(HEADERS_GUI
	gui/qmlimageprovider.h
)

set(SOURCES_GUI
        gui/barberpoleprogress.cpp
	gui/clickablelabel.cpp
	gui/ellipsislabel.cpp
	gui/customdockwidget.cpp
	gui/descriptionpanel.cpp
	gui/downloadscontrol.cpp
	gui/downloadsdelegate.cpp
	gui/downloadsform.cpp
	gui/faenzastyle.cpp
	gui/itemsdelegate.cpp
	gui/libraryform.cpp
	gui/maintoolbar.cpp
	gui/mainwindow.cpp
	gui/playerheader.cpp
	gui/preferences.cpp
	gui/searchresultform.cpp
	gui/sitescheckboxdelegate.cpp
	gui/sitescombo.cpp
	gui/spinner.cpp
	gui/videocontrol.cpp
	gui/videoplayerwidget.cpp
	gui/videoprogressbar.cpp
	gui/videoqualitydialog.cpp
	gui/videowidget.cpp
	gui/mousehoverbutton.cpp
	gui/previewpanelbutton.cpp
	gui/volumeprogressbar.cpp
)

set(FORMS_GUI
	gui/descriptionpanel.ui
	gui/downloadscontrol.ui
	gui/downloadsform.ui
	gui/libraryform.ui
	gui/maintoolbar.ui
	gui/mainwindow.ui
	gui/playerheader.ui
	gui/preferences.ui
	gui/searchresultform.ui
	gui/videocontrol.ui
	gui/videoqualitydialog.ui
)

source_group("gui\\sources" FILES
	${HEADERS_TOMOC_GUI}
	${HEADERS_GUI}
	${SOURCES_GUI}
)

source_group("gui\\forms" FILES
	${FORMS_GUI}
)


##############
# Logic group
##############

set(HEADERS_LOGIC
	logic/basefacademodel.h
	logic/errors.h
	logic/downloadmanager.h
	logic/configurableproxyfactory.h
	logic/entityfilenames.h
	logic/strategiescommon.h
)

set(HEADERS_TOMOC_LOGIC
	logic/downloadentity.h
	logic/downloadlistmodel.h
	logic/downloadsortfiltermodel.h	
	logic/librarymodel.h
	logic/libraryqmllistener.h
	logic/remotevideoentity.h
	logic/searchmanager.h
	logic/searchlistmodel.h
	logic/imagecache.h
	logic/libraryaddworker.h
	logic/libraryremoveworker.h
)

set(SOURCES_LOGIC
	logic/downloadentity.cpp
	logic/downloadlistmodel.cpp
	logic/downloadmanager.cpp
	logic/downloadsortfiltermodel.cpp	
	logic/errors.cpp
	logic/librarymodel.cpp
	logic/libraryqmllistener.cpp
	logic/remotevideoentity.cpp
	logic/searchmanager.cpp
	logic/searchlistmodel.cpp
	logic/configurableproxyfactory.cpp
	logic/imagecache.cpp
	logic/libraryaddworker.cpp
	logic/libraryremoveworker.cpp
)

source_group(logic FILES
	${HEADERS_LOGIC}
	${HEADERS_TOMOC_LOGIC}
	${SOURCES_LOGIC}
)

###################
# Scripting group #
###################

set(HEADERS_SCRIPTING
)

set(HEADERS_TOMOC_SCRIPTING
	scripting/scriptprovider.h
	scripting/scriptstrategy.h
)

set(SOURCES_SCRIPTING
	scripting/scriptprovider.cpp
	scripting/scriptstrategy.cpp
)

source_group(scripting FILES
	${HEADERS_SCRIPTING}
	${HEADERS_TOMOC_SCRIPTING}
	${SOURCES_SCRIPTING}
)

source_group("scripting\\strategies" FILES
	${JS_FILES}
)

###################
# QML group
###################
set(HEADERS_TOMOC_QML
	gui/qml/qdeclarativetoplevelitem.h
	gui/qml/qdeclarativetoplevelitem_p.h
	gui/qml/qrangemodel.h

)

set(HEADERS_QML
	gui/qml/qrangemodel_p.h
)

set(SOURCES_QML
	gui/qml/qdeclarativetoplevelitem.cpp
	gui/qml/qrangemodel.cpp
)

source_group("gui\\qml" FILES
	${HEADERS_TOMOC_QML}
	${HEADERS_QML}
	${SOURCES_QML}
)

############
# Assembly
############

set(HEADERS_TOMOC
	${HEADERS_TOMOC_GUI}
	${HEADERS_TOMOC_LOGIC}
	${HEADERS_TOMOC_SCRIPTING}
	${HEADERS_TOMOC_QML}
)

set(HEADERS
	${HEADERS_GUI}
	${HEADERS_LOGIC}
	${HEADERS_MAIN}
	${HEADERS_LOGIC}
	${HEADERS_SCRIPTING}
	${HEADERS_QML}
)

set(SOURCES
	${SOURCES_GUI}
	${SOURCES_LOGIC}
	${SOURCES_MAIN}
	${SOURCES_SCRIPTING}
	${SOURCES_QML}
)

set(FORMS
	${FORMS_GUI}
)

###################################################
###  END OF SECTION Sources
###################################################


###################################################
###  SECTION Generation
###################################################

QTX_ADD_RESOURCES(QRC_RESOURCES
	${RESOURCE_QT}
	${RESOURCE_QML}
)

QTX_WRAP_CPP(MOC_FILES
	${HEADERS_TOMOC}
)

QTX_WRAP_UI(UI_FILES
	${FORMS}
)

source_group(resources FILES
	${RESOURCE_QT}
	${RESOURCE_OS_IN}
	${RESOURCE_QML}
	${QML_FILES}
	${CSS_FILES}
)

source_group(".generated" FILES ${UI_FILES} ${MOC_FILES} ${QRC_RESOURCES})

###################################################
###  END OF SECTION Generation
###################################################


##################################################
###  SECTION Translations
###################################################

CREATE_BRANDING_HXX()

set(FILES_TO_TRANSLATE
	${FILES_TO_TRANSLATE}
	${CMAKE_CURRENT_SOURCE_DIR}/main/globals.h
	${SOURCES_GUI}
	${UI_FILES}
	${SOURCES_STRATEGIES}
	${SOURCES_LOGIC}
#	PARENT_SCOPE       -- do not write to parent scope, just read
)

#message(STATUS "FILES_TO_TRANSLATE=${FILES_TO_TRANSLATE}")

if(DEVELOPER_GENERATE_TRANSLATIONS)

	QTX_CREATE_TRANSLATION(QM_FILES 
		${TS_FILES}
		${FILES_TO_TRANSLATE}
		OPTIONS -no-obsolete
	)
	message("Generating translations")

else(DEVELOPER_GENERATE_TRANSLATIONS)

	QTX_ADD_TRANSLATION(QM_FILES 
		${TS_FILES}
	)

endif(DEVELOPER_GENERATE_TRANSLATIONS)

source_group(translations FILES
	${TS_FILES}
)
source_group("qm-files" FILES ${QM_FILES})

###################################################
###  END OF SECTION Translations
###################################################


###################################################
###  SECTION Build
###################################################

if(APPLE)
	SET_SOURCE_FILES_PROPERTIES(${QT_MACOSX_RESOURCES} PROPERTIES
		MACOSX_PACKAGE_LOCATION Resources
	)
	SET_SOURCE_FILES_PROPERTIES(${QT_MACOSX_IMAGES} PROPERTIES
		MACOSX_PACKAGE_LOCATION Resources/images
	)
	SET_SOURCE_FILES_PROPERTIES(${MACOSX_APPICON} PROPERTIES
		MACOSX_PACKAGE_LOCATION Resources
	)

elseif(WIN32)

	set_source_files_properties(${RESOURCE_OS} PROPERTIES LANGUAGE RC)

endif(APPLE)

set(PROJECT_LIBRARIES
	Utilities
	ui_utils
	QtSingleApplication
	Download
	Player
	ScriptEngine
	pythonqt
)

if(APPLE)
	set(PROJECT_LIBRARIES 
		${PROJECT_LIBRARIES} 
		Darwin
	)
endif()

if(DEVELOPER_TRAFFIC_CONTROL)
	set(PROJECT_LIBRARIES ${PROJECT_LIBRARIES}
		Traffic_Limitation
	)
endif(DEVELOPER_TRAFFIC_CONTROL)

# Configure DESKTOP_TARGET variable
SET_DESKTOP_TARGET()

add_library(${FUNCTIONAL_MODULE} STATIC
	# Sources
	${HEADERS_TOMOC}
	${HEADERS}
	${SOURCES}

	# Generated resources
	${UI_FILES} 
	${MOC_FILES} 
	${QRC_RESOURCES}
#	${RESOURCE_OS}
	${QM_FILES}

	# Some source resources
	${TS_FILES}
	${QML_FILES}
	${JS_FILES}
	${CSS_FILES}
)

# Add dependencies for the custom target so that the script runs before building
add_dependencies(${FUNCTIONAL_MODULE} update_version_file)

	set(QTX_LIBRARIES 
		${Qt5Core_LIBRARIES}
		${Qt5Gui_LIBRARIES}
		${Qt5Widgets_LIBRARIES}
		${Qt5Xml_LIBRARIES}
		${Qt5Network_LIBRARIES}
		${Qt5ScriptTools_LIBRARIES}
	)

	if(DEVELOPER_OPENGL)
		set(QTX_LIBRARIES 
			${QTX_LIBRARIES}
			${Qt5OpenGL_LIBRARIES}
		)
	endif(DEVELOPER_OPENGL)


target_link_libraries(${FUNCTIONAL_MODULE}
	Qt5::Quick
	${QTX_LIBRARIES}
	${EXTERNAL_LIBS}
	${PROJECT_LIBRARIES}
)

source_group("main" FILES  main/main.cpp ${ICON_FILE} ${RESOURCE_OS})

add_executable(${PROJECT_NAME}
	${DESKTOP_TARGET}
	main/main.cpp

	# Generated resources
	${QRC_RESOURCES}
	${RESOURCE_OS}
	${ICON_FILE}
)

	set(QTX_QTMAIN_LIBRARY ${Qt5Core_QTMAIN_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PRIVATE
	${QTX_QTMAIN_LIBRARY}
	${FUNCTIONAL_MODULE}
)

if(DEVELOPER_BUILD_TESTS)
	APPEND_COMMON_TESTS()

	QTX_ADD_RESOURCES(QRC__RESOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
	)

	macro(add_script_strategy_test STRATEGY_NAME)
		do_test(scriptstrategy_${STRATEGY_NAME}	"tests/scriptstrategies/test-scriptstrategies.cpp" ${QRC__RESOURCES})
		set_property(TARGET test_scriptstrategy_${STRATEGY_NAME} PROPERTY COMPILE_DEFINITIONS STRATEGY_NAME=\"${STRATEGY_NAME}\" MOC_FILE="test-scriptstrategies.scriptstrategy_${STRATEGY_NAME}.moc" QT_WIDGETS_LIB QT_GUI_LIB)
		set_property(TARGET test_scriptstrategy_${STRATEGY_NAME} PROPERTY FOLDER "Tests/ScriptStrategies")
	endmacro(add_script_strategy_test STRATEGY_NAME)

	add_script_strategy_test(YouTube)
	add_script_strategy_test(Vimeo)
	add_script_strategy_test(Dailymotion)


	# all do_test calls must be called here

endif(DEVELOPER_BUILD_TESTS)


add_dependencies(${PROJECT_NAME}
	${PROJECT_LIBRARIES}
)

###################################################
###  END OF SECTION Build
###################################################


###################################################
###  Set product version
###################################################

VersionConf(${PROJECT_NAME} ${RESOURCE_OS_IN} ${RESOURCE_OS})

if(WIN32)
	string(REPLACE "." ";" versionList ${PROJECT_VERSION})
	list(GET versionList 0 PROJECT_VERSION_MAJOR)
	list(GET versionList 1 PROJECT_VERSION_MINOR1)
	set(CMAKE_EXE_LINKER_FLAGS "/VERSION:\"${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR1}\"" CACHE STRING "Resetting the default linker options." FORCE)
	string(REPLACE "." "," FILEVERSION ${PROJECT_VERSION})
	set(VERSION_STR ${PROJECT_VERSION})
endif()

###################################################
###  END Set product version
###################################################


###################################################
###  SECTION Post Build
###################################################

#get_target_property(BINARY_FILE ${PROJECT_NAME} LOCATION)
#get_filename_component(BINARY_PATH ${BINARY_FILE} PATH)


# Start to install
INSTALL_IMAGEFORMATS(${PROJECT_NAME})
INSTALL_QT5PLUGINS(${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME} pythonqt DESTINATION ${BINARY_INSTALL_DIR})
TARGET_BUNDLEFIX(${PROJECT_NAME})
INSTALL_TRANSLATIONS(${PROJECT_NAME} ${QM_FILES})

if(WIN32)
  set(certFile "${CMAKE_SOURCE_DIR}/sign/${PROJECT_COMPANYNAME}/Cert.pfx")

  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND mt -manifest "${CMAKE_SOURCE_DIR}/resources/win7.manifest" -outputresource:$<TARGET_FILE:${PROJECT_NAME}>
    COMMAND cmd /c "if exist \"${certFile}\" (\"${CMAKE_COMMAND}\" -E echo Signing ${PROJECT_NAME} & signtool sign /v /f \"${certFile}\" \"$<TARGET_FILE:${PROJECT_NAME}>\")"
    VERBATIM
  )
elseif(APPLE)
	set_target_properties(${PROJECT_NAME} PROPERTIES 
		MACOSX_BUNDLE_ICON_FILE 
		${MACOSX_BUNDLE_ICON_FILE}
	)
endif(WIN32)


# Copy apple bz2 lib
#if(APPLE)
#	APPEND_DYNAMIC_LIB(${BZIP2_LIB})
#endif(APPLE)

# Install Qt and SSL libs

INSTALL_QT(${PROJECT_NAME} ${QT_COMPONENTS_TO_USE})
INSTALL_QT_PLUGINS(${PROJECT_NAME})
INSTALL_OPENSSL(${PROJECT_NAME})
INSTALL_FFMPEG_DLLS()
INSTALL_RUNTIME_LIBRARIES()

if(DEVELOPER_FEATURES)
	INSTALL_DEBUG_INFO_FILE()
endif(DEVELOPER_FEATURES)

# Unix icon and link installation
if(NOT APPLE AND UNIX)
	install(FILES ${RESOURCE_OS} DESTINATION share/applications/)
	install(FILES ${CMAKE_SOURCE_DIR}/resources/images/fvdownloader.png DESTINATION share/icons/hicolor/256x256/apps/)
endif(NOT APPLE AND UNIX)


# vcpkg

if(MSVC)
	set(INSTALL_BINARY_FILE "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.exe")

	get_filename_component(CMAKE_TOOLCHAIN_DIRECTORY ${CMAKE_TOOLCHAIN_FILE} DIRECTORY)

	if (VCPKG_TARGET_TRIPLET)
	    set( CMAKE_INSTALL_PLATFORM ${VCPKG_TARGET_TRIPLET} )
	elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
	    set( CMAKE_INSTALL_PLATFORM "x64-windows" )
	else ()
	    set( CMAKE_INSTALL_PLATFORM "x86-windows" )
	endif ()

	install(CODE "execute_process(COMMAND \"powershell\" \"-noprofile\" \"-executionpolicy\" \"Bypass\" \"-file\" \"${CMAKE_TOOLCHAIN_DIRECTORY}/msbuild/applocal.ps1\" \"-targetBinary\" \"${INSTALL_BINARY_FILE}\" \"-installedDir\" \"${CMAKE_TOOLCHAIN_DIRECTORY}/../../installed/${CMAKE_INSTALL_PLATFORM}/bin\" \"-OutVariable\" \"out\")")
endif()

###################################################
###  END OF SECTION Post Build
###################################################
