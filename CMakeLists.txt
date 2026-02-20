cmake_minimum_required(VERSION 3.16)
project(aerothemeplasma-kwin-components)

set(PROJECT_VERSION "6.5.4")
set(PROJECT_VERSION_MAJOR 6)
option(KWIN_INSTALL_MISC "Install miscellaneous components" ON)
option(KWIN_BUILD_WAYLAND "Build with Wayland support" OFF)

add_subdirectory(effects_cpp)

if(KWIN_INSTALL_MISC)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/smod/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/smod)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/kwin/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/kwin)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/outline DESTINATION ${CMAKE_INSTALL_PREFIX}/share/aerothemeplasma)
else()
    message(STATUS "Skipping other KWin components")
endif()

install()
