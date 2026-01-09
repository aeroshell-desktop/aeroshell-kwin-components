cmake_minimum_required(VERSION 3.16)
project(aerothemeplasma-kwin-components)

set(PROJECT_VERSION "6.5.4")
set(PROJECT_VERSION_MAJOR 6)

find_file(SMOD_DECORATION_H breezedecoration.h ${CMAKE_INSTALL_PREFIX}/include/SMOD/Decoration)
option(BUILD_SMODGLOW "Build smodglow which requires SMOD Decorations" ON)

if(SMOD_DECORATION_H)
    message("Found SMOD Decoration header, smodglow can be compiled.")
else()
    message("SMOD Decoration header was not found, smodglow will not be compiled.")
endif()

add_subdirectory(effects_cpp)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/smod/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/smod)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/kwin/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/kwin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/outline DESTINATION ${CMAKE_INSTALL_PREFIX}/share/aerothemeplasma)

install()
