#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "blosc_static" for configuration "Release"
set_property(TARGET blosc_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(blosc_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libblosc.a"
  )

list(APPEND _cmake_import_check_targets blosc_static )
list(APPEND _cmake_import_check_files_for_blosc_static "${_IMPORT_PREFIX}/lib/libblosc.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
