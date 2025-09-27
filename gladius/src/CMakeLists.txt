set(BINARY ${CMAKE_PROJECT_NAME})
file(GLOB SOURCES LIST_DIRECTORIES true *.h *.cpp)

# MCP sources are intentionally NOT part of the core targets (gladius, gladius_lib)
# They will be added to the dedicated gladiusmcp target below
file(GLOB MCP_SOURCES LIST_DIRECTORIES true mcp/*.h mcp/*.cpp)
# Add MCP tools sources
file(GLOB MCP_TOOLS_SOURCES LIST_DIRECTORIES true mcp/tools/*.h mcp/tools/*.cpp)



add_library(${BINARY}_lib STATIC ${SOURCES})
# MCP-enabled variant of the core library so Application.cpp and others are built with MCP code
add_library(${BINARY}_lib_mcp STATIC ${SOURCES})
target_compile_definitions(${BINARY}_lib_mcp PUBLIC GLADIUS_ENABLE_MCP)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/io
    ${CMAKE_CURRENT_SOURCE_DIR}/contour
    ${CMAKE_CURRENT_SOURCE_DIR}/nodes
    ${CMAKE_CURRENT_SOURCE_DIR}/compute
)

set (INCLUDE_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/io
    ${CMAKE_CURRENT_SOURCE_DIR}/contour
    ${CMAKE_CURRENT_SOURCE_DIR}/nodes
    ${CMAKE_CURRENT_SOURCE_DIR}/compute
    ${CMAKE_CURRENT_SOURCE_DIR}/components/IconFontCppHeaders
    ${CMAKE_CURRENT_SOURCE_DIR}/components
    ${OPENMESH_INCLUDE_DIRS}
    ${AdditionalIncludes}
)
target_include_directories(${BINARY}_lib PUBLIC ${INCLUDE_PATHS})
target_include_directories(${BINARY}_lib_mcp PUBLIC ${INCLUDE_PATHS})
target_include_directories(${BINARY} PUBLIC ${INCLUDE_PATHS})

# Core dependencies (shared by gladius and gladiusmcp)
set(DEPENDENCIES_CORE
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
    PkgConfig::libcoro
)

# MCP-only dependencies
set(DEPENDENCIES_MCP
    httplib::httplib
    nlohmann_json::nlohmann_json
)

if (WIN32)
    set(DEPENDENCIES_CORE ${DEPENDENCIES_CORE} blosc_shared)
else()
    set(DEPENDENCIES_CORE ${DEPENDENCIES_CORE} blosc_static)
endif()

if (ENABLE_TRACY)
    set(DEPENDENCIES_CORE ${DEPENDENCIES_CORE} Tracy::TracyClient)
endif()


target_link_libraries(${BINARY}_lib PUBLIC ${DEPENDENCIES_CORE})
target_link_libraries(${BINARY}_lib_mcp PUBLIC ${DEPENDENCIES_CORE})
target_link_libraries(${BINARY} PUBLIC ${BINARY}_lib)

# Enable AVX2 SIMD instructions for Phase 3 optimization
if (NOT WIN32)
    target_compile_options(${BINARY}_lib PRIVATE -mavx2 -mfma)
    target_compile_options(${BINARY} PRIVATE -mavx2 -mfma)
endif()

install(TARGETS ${BINARY} RUNTIME DESTINATION ${INSTALL_PREFIX}/)

#
# gladiusmcp: gladius with MCP server support
#
# Build gladiusmcp by linking the MCP-enabled core library and MCP sources
add_executable(gladiusmcp ${MCP_SOURCES} ${MCP_TOOLS_SOURCES} main.cpp)
target_include_directories(gladiusmcp PUBLIC ${INCLUDE_PATHS}
    ${CMAKE_CURRENT_SOURCE_DIR}/io
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/contour
    ${CMAKE_CURRENT_SOURCE_DIR}/nodes
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp
)
target_compile_definitions(gladiusmcp PRIVATE GLADIUS_ENABLE_MCP)
target_link_libraries(gladiusmcp PUBLIC ${BINARY}_lib_mcp ${DEPENDENCIES_MCP})
# Ensure the MCP-enabled static library is built before linking the executable
add_dependencies(gladiusmcp ${BINARY}_lib_mcp)
if (NOT WIN32)
    target_compile_options(gladiusmcp PRIVATE -mavx2 -mfma)
endif()
# Install MCP variant if it was built in this build tree; do not fail packaging if absent
install(PROGRAMS "$<TARGET_FILE:gladiusmcp>" DESTINATION ${INSTALL_PREFIX}/ OPTIONAL)
