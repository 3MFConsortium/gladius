
add_subdirectory(GladiusLib_component/Implementations/Cpp)

install(DIRECTORY GladiusLib_component/Bindings/ DESTINATION ${INSTALL_PREFIX}/bindings/)
install(DIRECTORY GladiusLib_component/Implementations/Cpp/Interfaces/ DESTINATION ${INSTALL_PREFIX}/bindings/CppDynamic/)
