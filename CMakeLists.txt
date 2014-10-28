
PROJECT( QAntenna CXX)

cmake_minimum_required(VERSION 2.8)

# TODO handle Qt5
FIND_PACKAGE( Qt4 4.6.3 REQUIRED QtCore QtGui QtOpenGL)
INCLUDE(${QT_USE_FILE})
ADD_DEFINITIONS(${QT_DEFINITIONS})


# http://blog.alexbeutel.com/285/cmake-opengl-and-glx-on-os-x/
# http://stackoverflow.com/questions/9460242/how-to-compile-glut-opengl-project-with-cmake-and-kdevelop-in-linux
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})


# skip warnings for the moment
# warning: 'gluErrorString' is deprecated: first deprecated in OS X 10.9
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated ")

# set debug mode
SET(CMAKE_BUILD_TYPE debug)

# Set the values of the defines
set (QANTENNA_VERSION "trunk")
ADD_DEFINITIONS(-DQANTENNA_VERSION=\"${QANTENNA_VERSION}\")
SET ( EXAMPLES_PATH  "" )
ADD_DEFINITIONS(-DEXAMPLES_PATH=\"${EXAMPLES_PATH}\")


ADD_SUBDIRECTORY( src )


#
# Custom uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)

