vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO 3DJan/lib3mf
    REF "3djan/volumetric_beamlattice_gradient"
    SHA512 b65032c81d05423713f2cda5fd8e80d7fcd4b0a225ca0192582678727d1be7d397dfed39c598ad07bc75ab8eddc7d8b1fd10c832c4d05adc81c266742c189d32
)

# Normalize target_link_libraries signature by inserting PRIVATE for plain-signature lines
file(READ "${SOURCE_PATH}/CMakeLists.txt" _lib3mf_cml)

# Prepare literal token refs to avoid expansion during replacement
set(_DLR "$")
string(CONCAT _PROJ_REF "${_DLR}" "{" "PROJECT_NAME" "}")
string(CONCAT _ZLIB_LIB_REF "${_DLR}" "{" "ZLIB_LIBRARIES" "}")
string(CONCAT _ZLIB_INC_REF "${_DLR}" "{" "ZLIB_INCLUDE_DIRS" "}")
string(CONCAT _ZLIB_LIBDIRS_REF "${_DLR}" "{" "ZLIB_LIBRARY_DIRS" "}")
string(CONCAT _LIBZIP_LIB_REF "${_DLR}" "{" "LIBZIP_LIBRARIES" "}")
string(CONCAT _LIBZIP_INC_REF "${_DLR}" "{" "LIBZIP_INCLUDE_DIRS" "}")
string(CONCAT _LIBZIP_LIBDIRS_REF "${_DLR}" "{" "LIBZIP_LIBRARY_DIRS" "}")

# Convert libzip linkage to keyword form and add include dirs
string(REGEX REPLACE
    [=[target_link_libraries\([ \t\r\n]*\$\{PROJECT_NAME\}[^)]*\$\{LIBZIP_LIBRARIES\}[^)]*\)]=]
    "target_link_libraries(${_PROJ_REF} PRIVATE ${_LIBZIP_LIB_REF})\n    target_include_directories(${_PROJ_REF} PRIVATE ${_LIBZIP_INC_REF})\n    target_link_directories(${_PROJ_REF} PRIVATE ${_LIBZIP_LIBDIRS_REF})"
    _lib3mf_cml "${_lib3mf_cml}")

# Convert zlib linkage to keyword form and add include dirs
string(REGEX REPLACE
    [=[target_link_libraries\([ \t\r\n]*\$\{PROJECT_NAME\}[^)]*\$\{ZLIB_LIBRARIES\}[^)]*\)]=]
    "target_link_libraries(${_PROJ_REF} PRIVATE ${_ZLIB_LIB_REF})\n    target_include_directories(${_PROJ_REF} PRIVATE ${_ZLIB_INC_REF})\n    target_link_directories(${_PROJ_REF} PRIVATE ${_ZLIB_LIBDIRS_REF})"
    _lib3mf_cml "${_lib3mf_cml}")

# Flip any hardcoded static CRT flags to dynamic to follow vcpkg defaults
string(REPLACE " /MTd" " /MDd" _lib3mf_cml "${_lib3mf_cml}")
string(REPLACE " /MT"  " /MD"  _lib3mf_cml "${_lib3mf_cml}")

file(WRITE "${SOURCE_PATH}/CMakeLists.txt" "${_lib3mf_cml}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS 
        -DUSE_INCLUDED_ZLIB=OFF
        -DUSE_INCLUDED_LIBZIP=OFF
        -DUSE_INCLUDED_SSL=OFF
        -DBUILD_FOR_CODECOVERAGE=OFF
        -DLIB3MF_TESTS=OFF
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/lib3mf)
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
