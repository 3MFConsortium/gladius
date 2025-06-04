
cmake_minimum_required(VERSION 3.5)

set(CMAKE_CURRENT_BINDING_DIR ${CMAKE_SOURCE_DIR}/src/api/GladiusLib_component/Bindings/CppDynamic/)

set(BINARY ${CMAKE_PROJECT_NAME}_integrationtest)
file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)

# Remove OpenCL-requiring tests if disabled
if(NOT ENABLE_OPENCL_TESTS)
    list(REMOVE_ITEM TEST_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/GladiusLib_tests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Slice_tests.cpp"
    )
    message(STATUS "OpenCL tests disabled - excluding OpenCL-requiring integration tests")
endif()

set(SOURCES ${TEST_SOURCES})
add_executable(${BINARY} ${TEST_SOURCES})
if (MSVC)
    set_property(TARGET ${BINARY} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set_property(TARGET ${BINARY} PROPERTY CXX_STANDARD 20)



message("CMAKE_CURRENT_BINDING_DIR=${CMAKE_CURRENT_BINDING_DIR}")
if (UNIX)
  target_link_libraries(${BINARY} PRIVATE ${CMAKE_DL_LIBS})
endif (UNIX)

set(DEPENDENCIES ${CMAKE_PROJECT_NAME}_lib GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main Eigen3::Eigen lodepng fmt::fmt)

if (ENABLE_TRACY)
    set(DEPENDENCIES ${DEPENDENCIES} Tracy::TracyClient)
endif()


target_link_libraries(${BINARY} PRIVATE ${CMAKE_PROJECT_NAME}_lib  ${DEPENDENCIES})
target_include_directories(${BINARY} PUBLIC ../../src ${CMAKE_CURRENT_BINDING_DIR})

file(GLOB TEST_FILES
  "testdata/*.*"
)

file(COPY ${TEST_FILES} DESTINATION "testdata")

file(GLOB FONT_FILES
  "../../components/imgui/misc/fonts/*.*"
)
file(COPY ${FONT_FILES}  DESTINATION "misc/fonts/")

gtest_add_tests(TARGET ${BINARY})