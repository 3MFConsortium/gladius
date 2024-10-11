vcpkg_from_github(
   OUT_SOURCE_PATH SOURCE_PATH
   REPO 3MFConsortium/lib3mf
   REF 80ecd20460f4b951cb0c0f6f35c2cbfd5466384d
   SHA512 778c455badde41092ca4b4e5e9788ce2e6fd05998927c23c21ac010bc09857bb8659c72cdd67a8c149adcb355d155b264ca39f6ac061745f12c6065f8a495261
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
