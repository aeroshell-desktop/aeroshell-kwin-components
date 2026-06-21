add_subdirectory(kcm)

set(aeroglassblur_SOURCES
    blur.cpp
    aeroblur.qrc
    main.cpp
)

kconfig_add_kcfg_files(aeroglassblur_SOURCES
    blurconfig.kcfgc
)

add_library(aeroglassblur-x11 MODULE ${aeroglassblur_SOURCES})

target_link_libraries(aeroglassblur-x11 PRIVATE
    KF6::ConfigGui
    KDecoration3::KDecoration
    KF6::Svg
    KWinX11::kwin
    X11::xcb
)

set_target_properties(aeroglassblur-x11 PROPERTIES OUTPUT_NAME aeroglassblur)

install(TARGETS aeroglassblur-x11 DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin-x11/effects/plugins)


