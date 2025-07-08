set(BINARY ${CMAKE_PROJECT_NAME})
file(GLOB SOURCES LIST_DIRECTORIES true *.h *.cpp)

# Explicitly add our new shortcut system files
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ShortcutManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ShortcutManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ShortcutSettingsDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ShortcutSettingsDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BackupManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BackupManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/BaseExportDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/BaseExportDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/TemplateExportDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/LayerBasedMeshExporter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/LayerBasedMeshExporter.cpp
)

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

install(TARGETS ${BINARY} RUNTIME DESTINATION ${INSTALL_PREFIX}/)
