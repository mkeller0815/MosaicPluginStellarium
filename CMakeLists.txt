cmake_minimum_required(VERSION 3.16)
project(MosaicPlanner VERSION 0.1.0 LANGUAGES CXX)

# Mosaic Planner - an external (dynamically loaded) plug-in for Stellarium.
#
# Required:
#   STELLARIUM_SOURCE_DIR  Stellarium source tree of EXACTLY the version you run
#                          (e.g. git tag v25.3). Only its headers are used.
# macOS:
#   STELLARIUM_APP         Path to Stellarium.app. The plug-in links against the
#                          executable (-bundle_loader) and the Qt frameworks inside
#                          the bundle, so only Qt *headers* are needed at build time.
#                          They must not be newer than the Qt shipped with Stellarium.
# Linux:
#   Stellarium symbols are resolved at load time; link against the same Qt as Stellarium.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(STELLARIUM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_deps/stellarium-src" CACHE PATH "Stellarium source tree (same version as the installed Stellarium)")
if(APPLE)
	set(STELLARIUM_APP "/Applications/Stellarium.app" CACHE PATH "Stellarium application bundle")
	if(NOT CMAKE_OSX_ARCHITECTURES)
		set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
	endif()
endif()

if(NOT EXISTS "${STELLARIUM_SOURCE_DIR}/src/core/StelModule.hpp")
	message(FATAL_ERROR "STELLARIUM_SOURCE_DIR does not point to a Stellarium source tree: ${STELLARIUM_SOURCE_DIR}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

# Tests for the pure geometry code (no Stellarium/Qt needed)
option(MOSAICPLANNER_BUILD_TESTS "Build the geometry tests" ON)
if(MOSAICPLANNER_BUILD_TESTS)
	enable_testing()
	add_executable(GeometryTest tests/GeometryTest.cpp src/MosaicGeometry.cpp)
	set_target_properties(GeometryTest PROPERTIES AUTOMOC OFF)
	target_include_directories(GeometryTest PRIVATE src)
	add_test(NAME GeometryTest COMMAND GeometryTest)
endif()

set(SOURCES
	src/MosaicGeometry.cpp
	src/MosaicGeometry.hpp
	src/OcularsBridge.cpp
	src/OcularsBridge.hpp
	src/MosaicPlanner.cpp
	src/MosaicPlanner.hpp
	src/gui/MosaicPlannerDialog.cpp
	src/gui/MosaicPlannerDialog.hpp
)

# Stellarium loads modules/<ID>/lib<ID>.dylib (.so on Linux)
add_library(MosaicPlanner MODULE ${SOURCES})
set_target_properties(MosaicPlanner PROPERTIES PREFIX "lib")
if(APPLE)
	set_target_properties(MosaicPlanner PROPERTIES SUFFIX ".dylib")
endif()

set(STEL "${STELLARIUM_SOURCE_DIR}")
target_include_directories(MosaicPlanner PRIVATE
	src
	src/gui
	${STEL}/src
	${STEL}/src/core
	${STEL}/src/core/modules
	${STEL}/src/core/planetsephems
	${STEL}/src/gui
	${STEL}/src/scripting
	${STEL}/plugins/Oculars/src
	${STEL}/plugins/Oculars/src/gui
)

# These must match the official Stellarium build: some of them change class layouts.
target_compile_definitions(MosaicPlanner PRIVATE
	ENABLE_SCRIPTING ENABLE_SCRIPT_CONSOLE ENABLE_SCRIPT_QML
	ENABLE_MEDIA ENABLE_GPS ENABLE_NLS ENABLE_SHOWMYSKY
	_USE_MATH_DEFINES QT_NO_DEBUG
	MOSAICPLANNER_VERSION="${PROJECT_VERSION}"
)

set(QT_MODULES Core Gui Widgets OpenGL OpenGLWidgets)
if(APPLE)
	if(NOT EXISTS "${STELLARIUM_APP}/Contents/MacOS/stellarium")
		message(FATAL_ERROR "STELLARIUM_APP does not point to Stellarium.app: ${STELLARIUM_APP}")
	endif()
	# Use only the headers of the local Qt; link against the frameworks shipped in Stellarium.app.
	foreach(m ${QT_MODULES})
		target_include_directories(MosaicPlanner PRIVATE $<TARGET_PROPERTY:Qt6::${m},INTERFACE_INCLUDE_DIRECTORIES>)
		target_compile_definitions(MosaicPlanner PRIVATE $<TARGET_PROPERTY:Qt6::${m},INTERFACE_COMPILE_DEFINITIONS>)
		target_link_libraries(MosaicPlanner PRIVATE "${STELLARIUM_APP}/Contents/Frameworks/Qt${m}.framework/Qt${m}")
	endforeach()
	target_link_options(MosaicPlanner PRIVATE
		"SHELL:-bundle_loader \"${STELLARIUM_APP}/Contents/MacOS/stellarium\""
		"-Wl,-rpath,@executable_path/../Frameworks")
	set(MOSAICPLANNER_INSTALL_DIR "$ENV{HOME}/Library/Application Support/Stellarium/modules/MosaicPlanner")
else()
	foreach(m ${QT_MODULES})
		target_link_libraries(MosaicPlanner PRIVATE Qt6::${m})
	endforeach()
	set(MOSAICPLANNER_INSTALL_DIR "$ENV{HOME}/.stellarium/modules/MosaicPlanner")
endif()

install(TARGETS MosaicPlanner LIBRARY DESTINATION "${MOSAICPLANNER_INSTALL_DIR}")
