cmake_minimum_required(VERSION 3.12)

# Set policies to avoid warnings
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)  # FindBoost module removal policy
endif()

if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)  # install() DESTINATION paths normalization
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(ENABLE_OPENCL_TESTS "Enable tests that require OpenCL" ON)

set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "15")

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
else()
  message("Not using build cache. You can enable it by setting USE_CACHE=ON")
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
    # Use the dynamic runtime (/MD) to match vcpkg-built third-party libraries
    # and avoid RuntimeLibrary mismatches during linking.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
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

# Include coverage support after project declaration so compiler info is available
include(cmake/Coverage.cmake)

# Ensure the 'install' target depends on building all targets first
# (so CPack does not try to install artifacts that were not built yet).
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY OFF)

if (MSVC)
    add_compile_options(/bigobj /utf-8 /Zc:__cplusplus)

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

# Ensure installed binaries look for co-located shared libraries first
if(UNIX)
  # Install tree: Use $ORIGIN so installed binaries in /opt/gladius/<version> resolve
  # bundled shared objects (e.g. our patched lib3mf) placed alongside them.
  set(CMAKE_INSTALL_RPATH "$ORIGIN")
  # Build tree: DO NOT force build RPATH to install RPATH. Tests run from the
  # build tree (e.g. build/tests/unittests) and previously failed to locate
  # lib3mf.so.2 when we set CMAKE_BUILD_WITH_INSTALL_RPATH ON (RPATH became
  # just $ORIGIN which points to the test directory that does not contain lib3mf).
  set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
  set(CMAKE_SKIP_BUILD_RPATH OFF)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()

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
find_package(muparser CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
pkg_check_modules(Clipper2 REQUIRED IMPORTED_TARGET Clipper2)
pkg_check_modules(libcoro REQUIRED IMPORTED_TARGET libcoro)


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

# Bundle lib3mf runtime shared objects on Linux so the app runs out-of-the-box
if(UNIX)
  # Resolve the location of the imported lib3mf target
  get_target_property(_lib3mf_loc_release lib3mf::lib3mf IMPORTED_LOCATION_RELEASE)
  get_target_property(_lib3mf_loc         lib3mf::lib3mf IMPORTED_LOCATION)
  set(_lib3mf_resolved "${_lib3mf_loc_release}")
  if(NOT _lib3mf_resolved)
    set(_lib3mf_resolved "${_lib3mf_loc}")
  endif()
  if(_lib3mf_resolved)
    get_filename_component(_lib3mf_dir "${_lib3mf_resolved}" DIRECTORY)
    if(EXISTS "${_lib3mf_dir}")
      # Bundle all versioned variants (lib3mf.so, lib3mf.so.2, etc.) into install prefix.
      file(GLOB _lib3mf_so_files "${_lib3mf_dir}/lib3mf.so*")
      if(_lib3mf_so_files)
        install(FILES ${_lib3mf_so_files} DESTINATION ${INSTALL_PREFIX})
      endif()
      # Ensure build-tree executables (tests, tools) can locate lib3mf at runtime
      # by appending its directory to the build RPATH. This fixes CI failures like:
      #   error while loading shared libraries: lib3mf.so.2: cannot open shared object file
      list(APPEND CMAKE_BUILD_RPATH "${_lib3mf_dir}")
      list(REMOVE_DUPLICATES CMAKE_BUILD_RPATH)
      set(CMAKE_BUILD_RPATH "${CMAKE_BUILD_RPATH}" CACHE STRING "RPATH for build tree executables" FORCE)
    endif()
  endif()
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

# Copy CNanoVDB.h from vcpkg OpenVDB installation instead of using manually maintained copy
find_path(NANOVDB_INCLUDE_DIR
    NAMES nanovdb/CNanoVDB.h
    HINTS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
    REQUIRED
)
configure_file(${NANOVDB_INCLUDE_DIR}/nanovdb/CNanoVDB.h src/kernel/CNanoVDB.h COPYONLY)
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
file(COPY library DESTINATION "src/")


file(COPY components/licenses DESTINATION "src/")




# Install fonts
install(DIRECTORY components/fonts/fontawesome-free-5.14.0-web/ DESTINATION ${INSTALL_PREFIX}/misc/fonts/)

# Install licenses
install(DIRECTORY components/licenses DESTINATION ${INSTALL_PREFIX}/)

# Install imgui.ini
install(FILES src/imgui.ini DESTINATION ${INSTALL_PREFIX})

# Icon installation and desktop integration assets (no build-time conversion)
if(UNIX)
  set(GLADIUS_ICON_NAME gladius)
  # Install pre-generated PNG icon to hicolor so Icon=gladius works via theme
  install(FILES ${CMAKE_SOURCE_DIR}/documentation/img/gladius.png DESTINATION /usr/share/icons/hicolor/256x256/apps/ RENAME ${GLADIUS_ICON_NAME}.png)
  set(GLADIUS_ICON ${GLADIUS_ICON_NAME})
endif()

# Linux desktop integration (GNOME/KDE): install .desktop file and icon into system directories
if(UNIX)
  # Generate desktop launcher file
  set(GLADIUS_DESKTOP_FILE "${PROJECT_BINARY_DIR}/gladius.desktop")
  # Paths used inside the desktop file
  set(GLADIUS_EXEC "/usr/bin/gladius")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/desktop/gladius.desktop.in ${GLADIUS_DESKTOP_FILE} @ONLY)
  # Install desktop entry for all users
  install(FILES ${GLADIUS_DESKTOP_FILE} DESTINATION /usr/share/applications)
endif()

# Install documentation - not yet available
# install(DIRECTORY ${CMAKE_SOURCE_DIR}/documentation/ DESTINATION ${INSTALL_PREFIX}/share/doc/)

# Install examples
install(DIRECTORY ${CMAKE_SOURCE_DIR}/examples/ DESTINATION ${INSTALL_PREFIX}/examples/)

# Install library
install(DIRECTORY ${CMAKE_SOURCE_DIR}/library/ DESTINATION ${INSTALL_PREFIX}/library/)

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
    PATTERN "*.txt"
    PATTERN "url"
    PATTERN "LICENSE.txt"
    PATTERN "LICENSE"
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

# Install a lightweight launcher in /usr/bin that forwards to the real binary in /opt
if(UNIX)
  set(GLADIUS_LAUNCHER "${PROJECT_BINARY_DIR}/gladius_launcher.sh")
  # Generate a launcher that ensures the bundled shared libraries (notably the version of lib3mf shipped with Gladius)
  file(WRITE "${GLADIUS_LAUNCHER}" "#!/bin/sh\n"
    "# Added by CMake: ensure Gladius' private libs (e.g., custom lib3mf) are found first.\n"
    "GLADIUS_DIR=\"${INSTALL_PREFIX}\"\n"
    "if [ -z \"\$LD_LIBRARY_PATH\" ]; then\n"
    "  export LD_LIBRARY_PATH=\"\$GLADIUS_DIR\"\n"
    "else\n"
    "  case :\$LD_LIBRARY_PATH: in\n"
    "    *:\$GLADIUS_DIR:*) ;;\n"
    "    *) export LD_LIBRARY_PATH=\"\$GLADIUS_DIR:\$LD_LIBRARY_PATH\" ;;\n"
    "  esac\n"
    "fi\n"
    "exec \"\$GLADIUS_DIR/gladius\" \"$@\"\n")
  install(PROGRAMS "${GLADIUS_LAUNCHER}" DESTINATION ${SYMLINK_BIN_DIR} RENAME gladius)
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
  # Let dpkg-shlibdeps compute runtime dependencies against system libs.
  # We still bundle lib3mf to avoid requiring it from apt when unavailable.
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)

# Convenience target to generate installer and ensure required binaries are built first
add_custom_target(Generate_Installer
  COMMAND "${CMAKE_CPACK_COMMAND}"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  # Make sure both desktop app variants are built before packaging
  DEPENDS gladius gladiusmcp gladius_lib
  COMMENT "Running CPack. Please wait...")

