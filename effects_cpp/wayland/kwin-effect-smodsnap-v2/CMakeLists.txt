cmake_minimum_required(VERSION 3.16.0)

project(effect_smodsnap)

option(KWIN_BUILD_WAYLAND "Build with Wayland support" ON)

set(QT_MAJOR_VERSION "6")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM ${QT_MAJOR_VERSION} REQUIRED NO_MODULE)

list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt${QT_MAJOR_VERSION} REQUIRED
    COMPONENTS
        Core
)

find_package(KF${QT_MAJOR_VERSION} REQUIRED
    COMPONENTS
        ConfigWidgets
)

find_package(KDecoration3 REQUIRED)

find_package(KWin REQUIRED COMPONENTS
    kwineffects
)

feature_summary(WHAT ALL)

set(kwin4_effect_smodsnap_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smodsnap-kf6.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smodsnap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smodsnap.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.cpp
)

add_library(kwin_effect_smodsnap SHARED ${kwin4_effect_smodsnap_SOURCES})

target_link_libraries(kwin_effect_smodsnap
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Core
        KF${QT_MAJOR_VERSION}::ConfigGui
        KDecoration3::KDecoration
        KWin::kwin
)

install(
    TARGETS
        kwin_effect_smodsnap
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins
)
