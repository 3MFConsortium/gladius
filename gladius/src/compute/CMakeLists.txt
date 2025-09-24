
set(BINARY ${CMAKE_PROJECT_NAME})
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES true *.h *.cpp)
target_sources(${BINARY}_lib PRIVATE ${SOURCES})
target_sources(${BINARY}_lib_mcp PRIVATE ${SOURCES})
target_sources(${BINARY} PRIVATE ${SOURCES})

target_include_directories(${BINARY}_lib PRIVATE ./)
target_include_directories(${BINARY}_lib_mcp PRIVATE ./)
target_include_directories(${BINARY} PRIVATE ./)