
# process Cards first
ADD_SUBDIRECTORY( cards )

# main source
INCLUDE_DIRECTORIES ( ${QAntenna_SOURCE_DIR}/src )

# http://www.cmake.org/Wiki/CMake/Tutorials/Qt
# https://qt-project.org/quarterly/view/using_cmake_to_build_qt_projects
# TODO push this down to srs/ui ?
INCLUDE_DIRECTORIES ( ${CMAKE_BINARY_DIR}/src ) # generated forms

SET(QAntenna_FORMS
  ${QAntenna_SOURCE_DIR}/src/ui/about.ui
  ${QAntenna_SOURCE_DIR}/src/ui/mainwindowqantenna.ui
)

SET(QAntenna_HEADERS
  ${QAntenna_SOURCE_DIR}/src/ui/about.h
  ${QAntenna_SOURCE_DIR}/src/ui/mainwindow.h
)

SET(QAntenna_SRC
  ${QAntenna_SOURCE_DIR}/src/ui/about.cpp
  ${QAntenna_SOURCE_DIR}/src/ui/mainwindow.cpp
)

SET(RESOURCES resources.qrc)

# files that have Q_OBJECT need to be MOC'ed
SET(MOC_HDRS
  antennadelegate.h
  datamanager.h
  glwidget.h
  loadthread.h
  neccontainer.h
  necinput.h
  necoutput.h
)

# TODO handle Qt5
QT4_WRAP_UI(QAntenna_FORMS_HEADERS ${QAntenna_FORMS})
QT4_WRAP_CPP(QAntenna_HEADERS_MOC ${QAntenna_HEADERS})
QT4_ADD_RESOURCES(RESOURCES_SRCS ${RESOURCES})
QT4_WRAP_CPP( MOC_SRCS ${MOC_HDRS} )


#
# TODO translation, update, deploy
#

SET(SOURCES
  gl.cpp
  camera.cpp
  datamanager.cpp
  glwidget.cpp
  line.cpp
  main.cpp
  neccontainer.cpp
  necinput.cpp
  necinputparser.cpp
  necoutput.cpp
  necoutputparser.cpp
  patch.cpp
  radiationpattern.cpp
  loadthread.cpp
  antennadelegate.cpp
  mmath.cpp
  vec3.cpp
  point4f.cpp
  vertexmatrix.cpp
)



ADD_EXECUTABLE( QAntenna MACOSX_BUNDLE WIN32
  ${SOURCES}
  ${MOC_SRCS}
  ${QAntenna_FORMS_HEADERS}
  ${QAntenna_HEADERS_MOC}
  ${QAntenna_SRC}
)

TARGET_LINK_LIBRARIES( QAntenna
  ${QT_LIBRARIES}
  ${OPENGL_LIBRARIES}
  QAntennaCards
)

#
# Install the application.
# On Mac the bundle is currenlty installed into bin.
#
INSTALL(TARGETS QAntenna
  BUNDLE DESTINATION bin COMPONENT Runtime
  RUNTIME DESTINATION bin COMPONENT Runtime
)

