add_subdirectory(kcm)

set(aeroglassblur_SOURCES
    blur.cpp
    main.cpp
    aeroblur.qrc
)

kconfig_add_kcfg_files(aeroglassblur_SOURCES
    blurconfig.kcfgc
)

add_library(aeroglassblur MODULE ${aeroglassblur_SOURCES})
target_compile_definitions(aeroglassblur PRIVATE "KWIN_BUILD_WAYLAND")
qt6_generate_wayland_protocol_server_sources(aeroglassblur
    "PRIVATE_CODE"
    FILES
        ${WaylandProtocols_DATADIR}/staging/ext-background-effect/ext-background-effect-v1.xml
)


target_link_libraries(aeroglassblur PRIVATE
    KF6::ConfigGui
    KDecoration3::KDecoration
    KF6::Svg
    KWin::kwin
)

install(TARGETS aeroglassblur DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins)
