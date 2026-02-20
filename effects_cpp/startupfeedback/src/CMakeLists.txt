#######################################
# Effect

# Source files
set(startupfeedback_SOURCES
    main.cpp
    startupfeedback.cpp
    startupfeedback.qrc
)

if (KWIN_BUILD_WAYLAND)
    add_library(launchfeedback MODULE ${startupfeedback_SOURCES})
    target_compile_definitions(launchfeedback PRIVATE "KWIN_BUILD_WAYLAND")
    
    target_link_libraries(launchfeedback PRIVATE
        KF6::ConfigCore
        KF6::ConfigGui
        Qt::DBus
        Qt::Widgets
        KF6::WindowSystem
        KWin::kwin
    )

    install(TARGETS launchfeedback DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins)
else()
    add_library(launchfeedback-x11 MODULE ${startupfeedback_SOURCES})
    #kwin_add_builtin_effect(startupfeedback ${startupfeedback_SOURCES})

    target_link_libraries(launchfeedback-x11 PRIVATE
        KF6::ConfigCore
        KF6::ConfigGui
        Qt::DBus
        Qt::Widgets
        KF6::WindowSystem
        KWinX11::kwin
    )

    set_target_properties(launchfeedback-x11 PROPERTIES OUTPUT_NAME x11/launchfeedback)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/x11/)

    install(TARGETS launchfeedback-x11 DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin-x11/effects/plugins)
endif()
