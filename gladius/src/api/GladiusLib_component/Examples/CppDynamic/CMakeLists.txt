#[[++

Copyright (C) 2021 Jan Orend

All rights reserved.

This file has been generated by the Automatic Component Toolkit (ACT) version 1.6.0.

Abstract: This is an autogenerated CMake Project that demonstrates the
 usage of the Dynamic C++ bindings of GladiusLib

Interface version: 1.0.0


]]

cmake_minimum_required(VERSION 3.5)

set(CMAKE_CURRENT_BINDING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Bindings/CppDynamic)
project(GladiusLibExample_CPPDynamic)
add_executable(GladiusLibExample_CPPDynamic "${CMAKE_CURRENT_SOURCE_DIR}/GladiusLib_example.cpp")
if (UNIX)
  target_link_libraries(GladiusLibExample_CPPDynamic ${CMAKE_DL_LIBS})
endif (UNIX)
target_include_directories(GladiusLibExample_CPPDynamic PRIVATE "${CMAKE_CURRENT_BINDING_DIR}")
