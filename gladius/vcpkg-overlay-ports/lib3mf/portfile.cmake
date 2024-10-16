vcpkg_from_github(
   OUT_SOURCE_PATH SOURCE_PATH
   REPO 3MFConsortium/lib3mf
   REF a12a270f79ebf1b04d2d03fa8b49e1aa8061aa56
   SHA512 99d30bd6babf0670994927c2162fdb31b11f2568a9b26d0305592dffa193f2a08f9dbc7bd80188288f090c6bedc1aa7da4640c019c545bfd93bfbc49fdb70953
   HEAD_REF implicit
)

vcpkg_from_github(
   OUT_SOURCE_PATH SOURCE_PATH_FASTFLOAT
   REPO fastfloat/fast_float
   REF v5.3.0
   SHA512 ebcbfbb277c8f1b183cfb3a5ad98357b1b349a7e8eb6780c98d99e607a4405b30c44568478188eeebaed96814c1ba9ad6c059853dce28630ef1a25c644f58571
   HEAD_REF master
)

# copy fast_float files to lib3mf/submodules/fast_float
file(COPY "${SOURCE_PATH_FASTFLOAT}/" DESTINATION "${SOURCE_PATH}/submodules/fast_float")

vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS 
        -DUSE_INCLUDED_ZLIB=ON
        -DUSE_INCLUDED_LIBZIP=ON
        -DUSE_INCLUDED_GTEST=OFF
        -DUSE_INCLUDED_SSL=OFF
        -DBUILD_FOR_CODECOVERAGE=OFF
        -DLIB3MF_TESTS=OFF
)

vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
# rename include/Bindings to include/lib3mf
file(RENAME "${CURRENT_PACKAGES_DIR}/include/Bindings" "${CURRENT_PACKAGES_DIR}/include/${PORT}")

# copy all cmake files from ${CURRENT_PACKAGES_DIR}/lib/cmake to share/${PORT}/
file(GLOB cmake_files "${CURRENT_PACKAGES_DIR}/lib/cmake/*")
file(COPY ${cmake_files} DESTINATION "${CURRENT_PACKAGES_DIR}/share/")

file(GLOB cmake_files_debug "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/*")
file(COPY ${cmake_files_debug} DESTINATION "${CURRENT_PACKAGES_DIR}/debug/share/")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/lib/cmake")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib/cmake")

vcpkg_fixup_cmake_targets()
vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
