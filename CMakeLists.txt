cmake_minimum_required(VERSION 3.16)
project(aeroshell-kwin-components)

set(PROJECT_VERSION "6.5.4")
set(PROJECT_VERSION_MAJOR 6)
option(KWIN_INSTALL_MISC "Install miscellaneous components" ON)
option(KWIN_BUILD_WAYLAND "Build with Wayland support" OFF)
set(KF6_MIN_VERSION "6.0.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMQtDeclareLoggingCategory)

set(PROJECT_VERSION "6.5.4")
set(PROJECT_VERSION_MAJOR 6)


include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 REQUIRED COMPONENTS
    Core Qml Quick DBus
)
find_package(KF6 REQUIRED COMPONENTS
    Config
)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
add_subdirectory(effects_cpp)

if(KWIN_INSTALL_MISC)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/smod/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/smod)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/kwin/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/kwin)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/outline DESTINATION ${CMAKE_INSTALL_PREFIX}/share/aeroshell)
    add_subdirectory(rules)
else()
    message(STATUS "Skipping other KWin components")
endif()

install()
