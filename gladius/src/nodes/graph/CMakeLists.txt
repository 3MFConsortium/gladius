
set(BINARY ${CMAKE_PROJECT_NAME})
file(GLOB_RECURSE SOURCES LIST_DIRECTORIES true *.h *.cpp)
target_sources(${BINARY}_lib PRIVATE ${SOURCES})
target_sources(${BINARY}_run PRIVATE ${SOURCES})

if(MSVC)
target_compile_options(${BINARY}_run PRIVATE /W3 /WX /D_USE_MATH_DEFINES)
target_compile_options(${BINARY}_lib PRIVATE /W3 /WX /D_USE_MATH_DEFINES)
else()
  target_compile_options(${BINARY}_run PRIVATE  )
  target_compile_options(${BINARY}_lib PRIVATE  )
endif()

target_include_directories(${BINARY}_lib PRIVATE ./)
target_include_directories(${BINARY}_run PRIVATE ./)