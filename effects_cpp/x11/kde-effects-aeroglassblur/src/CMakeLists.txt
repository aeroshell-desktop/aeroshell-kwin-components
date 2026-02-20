add_subdirectory(kcm)

set(aeroglassblur_SOURCES
    blur.cpp
    aeroblur.qrc
    main.cpp
)

kconfig_add_kcfg_files(aeroglassblur_SOURCES
    blurconfig.kcfgc
)

if (KWIN_BUILD_WAYLAND)
    add_library(aeroglassblur MODULE ${aeroglassblur_SOURCES})
    target_compile_definitions(aeroglassblur PRIVATE "KWIN_BUILD_WAYLAND")

    target_link_libraries(aeroglassblur PRIVATE
        KF6::ConfigGui
        KDecoration3::KDecoration
        KF6::Svg
        KWin::kwin
    )

    install(TARGETS aeroglassblur DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins)
else()
    add_library(aeroglassblur-x11 MODULE ${aeroglassblur_SOURCES})

    target_link_libraries(aeroglassblur-x11 PRIVATE
        KF6::ConfigGui
        KDecoration3::KDecoration
        KF6::Svg
        KWinX11::kwin
    )

    set_target_properties(aeroglassblur-x11 PROPERTIES OUTPUT_NAME x11/aeroglassblur)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/x11/)

    install(TARGETS aeroglassblur-x11 DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin-x11/effects/plugins)
endif()


