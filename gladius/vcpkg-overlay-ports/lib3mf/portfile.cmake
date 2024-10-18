vcpkg_from_github(
   OUT_SOURCE_PATH SOURCE_PATH
   REPO 3MFConsortium/lib3mf
   REF ed5e95e7ebf2125f197d1cd5b37cc0a7525c9494
   SHA512 c4a02dfab333e24705f4f1db18c95899a06e0d78c2b923dc8f6be892421230f2db2ebf799388eda3a56747b6c1107d3f19b4565414ed3aca061e76e8656fa507
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
