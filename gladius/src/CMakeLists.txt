set(BINARY ${CMAKE_PROJECT_NAME})
file(GLOB SOURCES LIST_DIRECTORIES true *.h *.cpp)

# Add MCP sources
file(GLOB MCP_SOURCES LIST_DIRECTORIES true mcp/*.h mcp/*.cpp)
# Add MCP tools sources
file(GLOB MCP_TOOLS_SOURCES LIST_DIRECTORIES true mcp/tools/*.h mcp/tools/*.cpp)
set(SOURCES ${SOURCES} ${MCP_SOURCES} ${MCP_TOOLS_SOURCES})



add_library(${BINARY}_lib STATIC ${SOURCES})

if (MSVC)
    add_executable(${BINARY} WIN32 ${SOURCES} )
    set_property(TARGET ${BINARY} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set_property(TARGET ${BINARY}_lib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    add_executable(${BINARY} ${SOURCES} )
endif()

# Add include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/components/IconFontCppHeaders
    ${CMAKE_CURRENT_SOURCE_DIR}/components/psimpl/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nodes
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compute
)

set (INCLUDE_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/components/IconFontCppHeaders
    ${CMAKE_CURRENT_SOURCE_DIR}/components
    ${OPENMESH_INCLUDE_DIRS}
    ${AdditionalIncludes}
)
target_include_directories(${BINARY}_lib PUBLIC ${INCLUDE_PATHS})
target_include_directories(${BINARY} PUBLIC ${INCLUDE_PATHS})

set(DEPENDENCIES
    OpenCL::OpenCL
    ${X11_LIBRARIES}
    ${OPENGL_LIBRARIES}
    glfw
    glad::glad
    fmt::fmt-header-only
    Eigen3::Eigen
    tinyfiledialogs::tinyfiledialogs
    Threads::Threads
    sago::platform_folders
    OpenVDB::openvdb
    ZLIB::ZLIB
    gladius::rc
    lodepng
    pugixml::pugixml
    ${OPENMESH_LIBRARIES}
    lib3mf::lib3mf
    unofficial::minizip::minizip
    imgui::imgui
    unofficial::imgui-node-editor::imgui-node-editor
    PkgConfig::Clipper2
    muparser::muparser
    httplib::httplib
    nlohmann_json::nlohmann_json
    PkgConfig::libcoro
)

if (WIN32)
    set(DEPENDENCIES ${DEPENDENCIES} blosc_shared)
else()
    set(DEPENDENCIES ${DEPENDENCIES} blosc_static)
endif()

if (ENABLE_TRACY)
    set(DEPENDENCIES ${DEPENDENCIES} Tracy::TracyClient)
endif()


target_link_libraries(${BINARY}_lib PUBLIC ${DEPENDENCIES})
target_link_libraries(${BINARY} PUBLIC ${BINARY}_lib)

# Enable AVX2 SIMD instructions for Phase 3 optimization
if (NOT WIN32)
    target_compile_options(${BINARY}_lib PRIVATE -mavx2 -mfma)
    target_compile_options(${BINARY} PRIVATE -mavx2 -mfma)
endif()

install(TARGETS ${BINARY} RUNTIME DESTINATION ${INSTALL_PREFIX}/)
