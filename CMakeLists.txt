cmake_minimum_required(VERSION 2.8)

project(vline)
add_executable(vline main.cpp)
find_package(OpenGL REQUIRED)
#find_package(GLUT REQUIRED)
#find_package(GLEW 2.0 REQUIRED)
#find_package(FREEGLUT REQUIRED)
include_directories( ${OPENGL_INCLUDE_DIRS}  ${GLUT_INCLUDE_DIRS} )

set_target_properties(vline
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(vline ${OPENGL_LIBRARIES} GLESv2 EGL X11)
