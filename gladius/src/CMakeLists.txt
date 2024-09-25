
set(BINARY ${CMAKE_PROJECT_NAME})
file(GLOB SOURCES LIST_DIRECTORIES true *.h *.cpp)

add_library(${BINARY}_lib STATIC ${SOURCES})

if (MSVC)
    add_executable(${BINARY} WIN32 ${SOURCES} )
    set_property(TARGET ${BINARY} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set_property(TARGET ${BINARY}_lib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    add_executable(${BINARY} ${SOURCES} )
endif()

set (INCLUDE_PATHS
    ../
    ../components/IconFontCppHeaders
    ../components/psimpl/lib
    ${STB_INCLUDE_DIRS}
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
    minizip::minizip
    imgui::imgui
    unofficial::imgui-node-editor::imgui-node-editor
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

install(TARGETS ${BINARY} RUNTIME DESTINATION .)
