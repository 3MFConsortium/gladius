
set(BINARY ${CMAKE_PROJECT_NAME})
file(GLOB_RECURSE SOURCES LIST_DIRECTORIES true *.h *.cpp)
target_sources(${BINARY}_lib PRIVATE ${SOURCES})
target_sources(${BINARY} PRIVATE ${SOURCES})

target_include_directories(${BINARY}_lib PRIVATE ./ ../../components/nanovdb)
target_include_directories(${BINARY} PRIVATE ./ ../../components/nanovdb)