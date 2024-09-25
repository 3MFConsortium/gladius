
add_subdirectory(GladiusLib_component/Implementations/Cpp)

install(DIRECTORY GladiusLib_component/Bindings/ DESTINATION bindings/)
install(DIRECTORY GladiusLib_component/Implementations/Cpp/Interfaces/ DESTINATION bindings/CppDynamic/)
