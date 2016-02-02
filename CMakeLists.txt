cmake_minimum_required(VERSION 2.6)
project(Robot)

set(EXECUTABLE_NAME Robot)

# Detect and add SFML
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})
#Find any version 2.X of SFML
#See the FindSFML.cmake file for additional details and instructions
find_package(SFML 2 REQUIRED system window graphics network audio)


if(SFML_FOUND)
   include_directories(${SFML_INCLUDE_DIR})

   # Enable debug symbols by default
   if(CMAKE_BUILD_TYPE STREQUAL "")
     set(CMAKE_BUILD_TYPE Debug)
   endif()
   # (you can also set it on the command line: -D CMAKE_BUILD_TYPE=Release)

   # Set version information in a config.h file
   set(Robot_VERSION_MAJOR 0)
   set(Robot_VERSION_MINOR 1)

   include_directories("${PROJECT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/include" "${PROJECT_BINARY_DIR}/lib")

   set(LIBRARY_NAME sfimgui)
   add_subdirectory(lib)

   set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/build)
   set(CMAKE_CXX_FLAGS "-g -Wall")

   add_subdirectory(src)
endif()
