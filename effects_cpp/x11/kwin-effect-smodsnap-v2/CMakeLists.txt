set(TARGET_NAME "kwin_effect_smodsnap")

set(kwin4_effect_smodsnap_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smodsnap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smodsnap.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smodsnap-kf6.cpp
)

add_library(${TARGET_NAME} SHARED ${kwin4_effect_smodsnap_SOURCES})

target_link_libraries(${TARGET_NAME}
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Core
        KF${QT_MAJOR_VERSION}::ConfigGui
        KDecoration3::KDecoration
        KWinX11::kwin
)

install(
    TARGETS
        ${TARGET_NAME}
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin-x11/effects/plugins
)
