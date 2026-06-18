cmake_minimum_required(VERSION 3.16)
project(aeroshell-kwin-components)

set(PROJECT_VERSION "6.6.5")
set(PROJECT_VERSION_MAJOR 6)
set(KF6_MIN_VERSION "6.0.0")

option(KWIN_INSTALL_MISC "Install miscellaneous components" ON)
option(KWIN_BUILD_WAYLAND "Build with Wayland support" OFF)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Gui Quick Widgets Core
    DBus UiTools Widgets OpenGL
    Network Xml WaylandClientPrivate WaylandClient
)

include_directories(${Qt6Widgets_INCLUDE_DIRS} ${Qt6Network_INCLUDE_DIRS} ${Qt6OpenGL_INCLUDE_DIRS} ${Qt6Xml_INCLUDE_DIRS})
add_definitions(${Qt6Widgets_DEFINITIONS})

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Config ConfigWidgets CoreAddons Crash
    I18n KIO Service Notifications
    WidgetsAddons WindowSystem GuiAddons KCMUtils
    Svg
)

find_package(KDecoration3 REQUIRED)

if(KWIN_BUILD_WAYLAND)
    find_package(KWin REQUIRED COMPONENTS
        kwineffects
    )
else()
    find_package(X11 REQUIRED)
    find_package(XCB REQUIRED COMPONENTS XCB)

    find_package(KWinX11 REQUIRED COMPONENTS
        kwineffects
    )
endif()


find_package(KWinDBusInterface CONFIG REQUIRED)

find_package(epoxy REQUIRED)
find_package(WaylandProtocols 1.48)
set_package_properties(WaylandProtocols PROPERTIES
    TYPE REQUIRED
    PURPOSE "Collection of Wayland protocols that add functionality not available in the Wayland core protocol"
    URL "https://gitlab.freedesktop.org/wayland/wayland-protocols/"
)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
add_subdirectory(effects_cpp)

if(KWIN_INSTALL_MISC)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/smod/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/smod)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/kwin/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/kwin)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/outline DESTINATION ${CMAKE_INSTALL_PREFIX}/share/aeroshell)
    add_subdirectory(rules)
    ki18n_install(po)
else()
    message(STATUS "Skipping other KWin components")
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

install()
