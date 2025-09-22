cmake_minimum_required(VERSION 3.12)

include(GoogleTest)
include(CTest) # Add this line
enable_testing()

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
    "${CMAKE_CURRENT_SOURCE_DIR}/BeamLatticeOpenCL_tests.cpp"
  )
  message(STATUS "OpenCL tests disabled - excluding OpenCL-requiring unit tests")
endif()

set(SOURCES ${TEST_SOURCES})
add_executable(${BINARY} ${TEST_SOURCES})

# Include MCP sources for tests that validate MCP behavior
file(GLOB TEST_MCP_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/mcp/*.h" "${CMAKE_SOURCE_DIR}/src/mcp/*.cpp"
  "${CMAKE_SOURCE_DIR}/src/mcp/tools/*.h" "${CMAKE_SOURCE_DIR}/src/mcp/tools/*.cpp"
)
target_sources(${BINARY} PRIVATE ${TEST_MCP_SOURCES})
target_compile_definitions(${BINARY} PRIVATE GLADIUS_ENABLE_MCP)

if(MSVC)
 target_compile_options(${BINARY} PRIVATE  /W3 /WX /D_USE_MATH_DEFINES)
 set_property(TARGET ${BINARY} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
 # Override project-level Windows subsystem setting for test executables that need console I/O
 set_target_properties(${BINARY} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

# Ensure C++20 standard is explicitly set for tests (required for structured bindings and other features)
set_property(TARGET ${BINARY} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${BINARY} PROPERTY CXX_STANDARD_REQUIRED ON)

target_include_directories(${BINARY} PUBLIC ../../src ../../src/io ../../components/nanovdb)

set(DEPENDENCIES ${CMAKE_PROJECT_NAME}_lib GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main ${FMT_LIBRARY} ${OPENMESH_LIBRARIES} Eigen3::Eigen lodepng Boost::boost unofficial::minizip::minizip OpenVDB::openvdb)

# Add MCP-only dependencies for tests that use MCP
list(APPEND DEPENDENCIES httplib::httplib nlohmann_json::nlohmann_json)

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

# Ensure licenses are available for dialogs or setup code that scans getAppDir()/licenses
file(COPY "../../components/licenses" DESTINATION ".")

# Provide example template used by Document::newFromTemplate()
file(MAKE_DIRECTORY "examples")
file(COPY "../../examples/template.3mf" DESTINATION "examples/")

# Use gtest_discover_tests for automatic test discovery
gtest_discover_tests(${BINARY})
