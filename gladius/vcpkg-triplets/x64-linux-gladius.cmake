set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Some packages may need to remain static for compatibility
if ("${VCPKG_CURRENT_PACKAGE}" STREQUAL "minizip")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()

if ("${VCPKG_CURRENT_PACKAGE}" STREQUAL "lz4")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()

if ("${VCPKG_CURRENT_PACKAGE}" STREQUAL "zlib")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()