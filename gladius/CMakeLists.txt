cmake_minimum_required(VERSION 3.12)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "12")

# Set the version of the package in src/version.h
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h"
)
#vcpgk.json
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json"
)

if (USE_CACHE)
find_program(buildcache_program buildcache)
if(buildcache_program)
  message("Using build cache " ${buildcache_program}) 
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${buildcache_program}")
else()
  message("buildcache not found")
endif()
endif() # USE_CACHE
message("CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
message("VCPKG_ROOT=${VCPKG_ROOT}")
message("ENV{VCPKG_ROOT}=$ENV{VCPKG_ROOT}")

if(DEFINED ENV{VCPKG_ROOT} AND NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

message("CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/components/cmrc)

set(_change_MSVC_flags FALSE)
if(WIN32)
  if(CMAKE_VERSION VERSION_LESS 3.15.0)
    set(_change_MSVC_flags TRUE)
  else()
    # Set MSVC runtime to MultiThreaded (/MT)
    cmake_policy(SET CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()

if(_change_MSVC_flags)
  # Modify compile flags to change MSVC runtime from /MD to /MT
  set(_re_match "([\\/\\-]M)D")
  set(_re_replace "\\1T")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

project(gladius)
if (MSVC)
    add_compile_options(/bigobj /utf-8 /Zc:char8_t- /Zc:__cplusplus) # see https://github.com/ocornut/imgui/issues/4866

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/RTC1) 
    endif()


    # /Zi is not supported by buildcache, so we have to replace it with /Z7
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
      add_compile_options(/Zi) # in release mode, we want to generate .pdb files
    else()
      string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
      string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
      string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
      string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    endif()
    if (ENABLE_ASAN)
        add_compile_options(/fsanitize=address)        
    endif()

    # no console
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()
add_definitions(-DTBB_SUPPRESS_DEPRECATED_MESSAGES -DNOMINMAX)

if (ENABLE_TRACY)
    add_definitions(-DTRACY_ENABLE -DTRACY_ONLY_LOCALHOST)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(OpenGL_GL_PREFERENCE GLVND)
enable_testing()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(CMakeRC CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenCL REQUIRED)
find_package(fmt REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(OpenMesh REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)
find_package(platform_folders REQUIRED)
if(UNIX)
find_package(X11 REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
endif()
find_package(OpenVDB CONFIG REQUIRED)
find_package(blosc CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(lodepng CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(lib3mf CONFIG REQUIRED)
find_package(unofficial-minizip CONFIG REQUIRED)
if (ENABLE_TRACY)
  find_package(Tracy CONFIG REQUIRED)
endif()
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-imgui-node-editor CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
pkg_check_modules(Clipper2 REQUIRED IMPORTED_TARGET Clipper2)


file(GLOB KERNEL_FILES
  "src/kernel/*"
)

cmrc_add_resource_library(gladius_resources ALIAS gladius::rc ${KERNEL_FILES})
if (MSVC)
    set_property(TARGET gladius_resources PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(AdditionalIncludes 
 ${CMAKE_CURRENT_SOURCE_DIR}/src
#  ${CMAKE_CURRENT_SOURCE_DIR}/components/imgui
 ${CMAKE_CURRENT_SOURCE_DIR}/components/IconFontCppHeaders
 ${CMAKE_CURRENT_SOURCE_DIR}/components/imgui-node-editor
 ${CMAKE_CURRENT_SOURCE_DIR}/components/psimpl
 ${CMAKE_CURRENT_SOURCE_DIR}/components/
)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(GLADIUSLOCATION GladiusLib)
message("GLADIUSLOCATION=${GLADIUSLOCATION}")

# Determine the installation directories based on the platform
if(WIN32)
    set(INSTALL_PREFIX .)
else()
    set(INSTALL_PREFIX /opt/gladius/${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})
    set(INSTALL_BIN_DIR ${INSTALL_PREFIX})
    set(SYMLINK_BIN_DIR /usr/bin)
endif()

add_subdirectory(tests)
add_subdirectory(src)
add_subdirectory(src/compute)
add_subdirectory(src/ui)
add_subdirectory(src/nodes)
add_subdirectory(src/io)
add_subdirectory(src/contour)
add_subdirectory(src/api)
add_subdirectory(src/api/GladiusLib_component/Examples/CppDynamic)

configure_file(src/kernel/sdf.cl src/kernel/sdf.cl COPYONLY)
configure_file(src/kernel/sdf.h src/kernel/sdf.h COPYONLY)
configure_file(src/kernel/rendering.cl src/kernel/rendering.cl COPYONLY)
configure_file(src/kernel/sdf_generator.cl src/kernel/sdf_generator.cl COPYONLY)
configure_file(src/kernel/CNanoVDB.h src/kernel/CNanoVDB.h COPYONLY)
configure_file(components/fonts/Roboto-Medium.ttf src/misc/fonts/Roboto-Medium.ttf COPYONLY)
configure_file(components/fonts/fontawesome-free-5.14.0-web/fa-regular-400.ttf src/misc/fonts/fa-regular-400.ttf COPYONLY)
configure_file(components/fonts/fontawesome-free-5.14.0-web/fa-solid-900.ttf src/misc/fonts/fa-solid-900.ttf COPYONLY)
configure_file(src/imgui.ini src/imgui.ini COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/src/gladius.ico src/gladius.ico COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/src/gladius.rc src/gladius.rc COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/src/gladius.rc.res gladius.rc.res COPYONLY)

# currently out of date
# configure_file(${CMAKE_SOURCE_DIR}/src/api/GladiusLib_component/Examples/Python/GladiusLib_Example.py src/api/GladiusLib_component/Examples/Python/GladiusLib_Example.py COPYONLY)
# configure_file(${CMAKE_SOURCE_DIR}/src/api/GladiusLib_component/Examples/Python/ToyTrack.py src/api/GladiusLib_component/Examples/Python/ToyTrack.py COPYONLY)
# configure_file(${CMAKE_SOURCE_DIR}/src/api/GladiusLib_component/Bindings/Python/GladiusLib.py src/api/GladiusLib_component/Examples/Python/GladiusLib.py COPYONLY)


file(COPY examples DESTINATION "src/")

file(COPY components/licenses DESTINATION "src/")




# Install fonts
install(DIRECTORY components/fonts/fontawesome-free-5.14.0-web/ DESTINATION ${INSTALL_PREFIX}/misc/fonts/)

# Install licenses
install(DIRECTORY components/licenses DESTINATION ${INSTALL_PREFIX}/doc/licenses/)

# Install imgui.ini
install(FILES src/imgui.ini DESTINATION ${INSTALL_PREFIX})

# Install icon
install(FILES ${CMAKE_SOURCE_DIR}/documentation/img/gladius.ico DESTINATION ${INSTALL_PREFIX}/share/pixmaps/)

# Install documentation - not yet available
# install(DIRECTORY ${CMAKE_SOURCE_DIR}/documentation/ DESTINATION ${INSTALL_PREFIX}/share/doc/)

# Install examples
install(DIRECTORY ${CMAKE_SOURCE_DIR}/examples/ DESTINATION ${INSTALL_PREFIX}/examples/)

# Install binaries and libraries
install(
    DIRECTORY ${PROJECT_BINARY_DIR}/src/
    DESTINATION ${INSTALL_PREFIX}
    FILES_MATCHING 
    PATTERN "*.dll"
    REGEX ".*\\.so\\.[0-9]+"
    PATTERN "*.so"
    PATTERN "*.ttf"
    PATTERN "*.rtf"
    PATTERN "url"
    PATTERN "LICENSE.txt"
    PATTERN "CMakeFiles" EXCLUDE
    PATTERN "vdb" EXCLUDE
    PATTERN "nodes" EXCLUDE
    PATTERN "contour" EXCLUDE
    PATTERN "ui" EXCLUDE
    PATTERN "api" EXCLUDE
    PATTERN "compute" EXCLUDE
    PATTERN "io" EXCLUDE
    PATTERN "kernel" EXCLUDE
)

# Create a symbolic link in /usr/bin
if(UNIX)
    install(CODE "execute_process(COMMAND ln -sf ${INSTALL_BIN_DIR}/gladius ${SYMLINK_BIN_DIR}/gladius)")
endif()


set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Gladius is a framework for processing 3MF files using the Volumetric and Implicit Extension.")
set(CPACK_PACKAGE_DESCRIPTION "Gladius is a framework for processing 3MF files using the Volumetric and Implicit Extension.")
set(CPACK_PACKAGE_VENDOR "3MF Consortium")
set(CPACK_PACKAGE_EXECUTABLES  gladius;Gladius)
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY ${PROJECT_NAME})
set(CPACK_SOURCE_IGNORE_FILES ${CPACK_IGNORE_FILES})
set(CPACK_PACKAGE_ICON ${CMAKE_SOURCE_DIR}/documentation/img/gladius.ico)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE.txt)

if(WIN32 AND NOT UNIX)
  set(CPACK_GENERATOR WIX)
  set(CPACK_WIX_UPGRADE_GUID "9A7EF5B0-52BD-42C3-88E6-58650EC3E993")
  set(CPACK_WIX_UI_BANNER ${CMAKE_SOURCE_DIR}/documentation/img/banner.png)
  set(CPACK_WIX_UI_DIALOG ${CMAKE_SOURCE_DIR}/documentation/img/dialog.png)
  set(CPACK_WIX_PRODUCT_ICON ${CMAKE_SOURCE_DIR}/documentation/img/gladius.ico)
  set(CPACK_WIX_PROGRAM_MENU_FOLDER .)
endif()

if (UNIX)
  set(CPACK_GENERATOR DEB)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "3MF Consortium")
  set(CPACK_DEBIAN_PACKAGE_SECTION "Graphics")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/3MFConsortium/gladius")
endif()

include(CPack)
add_custom_target( Generate_Installer
            COMMAND "${CMAKE_CPACK_COMMAND}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS gladius gladiuslib
            COMMENT "Running CPack. Please wait...")
