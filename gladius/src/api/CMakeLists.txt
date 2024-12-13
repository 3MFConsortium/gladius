
add_subdirectory(GladiusLib_component/Implementations/Cpp)

install(DIRECTORY ${INSTALL_PREFIX}/GladiusLib_component/Bindings/ DESTINATION bindings/)
install(DIRECTORY ${INSTALL_PREFIX}/GladiusLib_component/Implementations/Cpp/Interfaces/ DESTINATION bindings/CppDynamic/)
