cmake_minimum_required(VERSION 3.12)

include(GoogleTest)

set(BINARY ${CMAKE_PROJECT_NAME}_test)
file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)

# Remove OpenCL-requiring tests if disabled
if(NOT ENABLE_OPENCL_TESTS)
    list(REMOVE_ITEM TEST_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/Assembly_tests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ComputeCore_tests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/CliReader_tests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ToCommandStreamVisitor_tests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Builder_tests.cpp"
    )
    message(STATUS "OpenCL tests disabled - excluding OpenCL-requiring unit tests")
endif()

set(SOURCES ${TEST_SOURCES})
add_executable(${BINARY} ${TEST_SOURCES})

if(MSVC)
 target_compile_options(${BINARY} PRIVATE  /W3 /WX /D_USE_MATH_DEFINES)
 set_property(TARGET ${BINARY} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
target_include_directories(${BINARY} PUBLIC ../../src ../../components/nanovdb)

set(DEPENDENCIES ${CMAKE_PROJECT_NAME}_lib GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main ${FMT_LIBRARY} ${OPENMESH_LIBRARIES} Eigen3::Eigen lodepng Boost::boost unofficial::minizip::minizip)

if (ENABLE_TRACY)
    set(DEPENDENCIES ${DEPENDENCIES} Tracy::TracyClient)
endif()

target_link_libraries(${BINARY} PUBLIC
   ${CMAKE_PROJECT_NAME}_lib
   ${DEPENDENCIES}
   )



file(GLOB TEST_FILES
  "testdata/*.*"
)

file(COPY ${TEST_FILES} DESTINATION "testdata")

file(GLOB FONT_FILES
  "../../components/imgui/misc/fonts/*.*"
)
file(COPY ${FONT_FILES}  DESTINATION "misc/fonts/")


gtest_add_tests(TARGET ${BINARY})