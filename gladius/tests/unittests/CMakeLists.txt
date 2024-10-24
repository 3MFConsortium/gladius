cmake_minimum_required(VERSION 3.12)

include(GoogleTest)

set(BINARY ${CMAKE_PROJECT_NAME}_test)
file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)
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

# if(WIN32 AND NOT UNIX)
# configure_file(${CMAKE_SOURCE_DIR}/components/lib3mf/Bin/lib3mf.dll lib3mf.dll COPYONLY)
# endif()

# if(WIN32 AND NOT UNIX)
# configure_file(${CMAKE_SOURCE_DIR}/components/lib3mf/Bin/lib3mf.dll src/lib3mf.dll COPYONLY)
# else()
# configure_file(${CMAKE_SOURCE_DIR}/components/lib3mf/Bin/lib3mf.so src/lib3mf.so COPYONLY)
# configure_file(${CMAKE_SOURCE_DIR}/components/lib3mf/Bin/lib3mf.so src/lib3mf.so.2 COPYONLY)
# endif()

gtest_add_tests(TARGET ${BINARY})