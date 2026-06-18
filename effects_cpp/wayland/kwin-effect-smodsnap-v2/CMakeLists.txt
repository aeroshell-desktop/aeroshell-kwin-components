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
