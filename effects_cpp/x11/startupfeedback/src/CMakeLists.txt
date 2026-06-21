#######################################
# Effect

# Source files
set(startupfeedback_SOURCES
    main.cpp
    startupfeedback.cpp
    startupfeedback.qrc
)

add_library(launchfeedback-x11 MODULE ${startupfeedback_SOURCES})

target_link_libraries(launchfeedback-x11 PRIVATE
    KF6::ConfigCore
    KF6::ConfigGui
    Qt::DBus
    Qt::Widgets
    KF6::WindowSystem
    KWinX11::kwin
)

set_target_properties(launchfeedback-x11 PROPERTIES OUTPUT_NAME launchfeedback)

install(TARGETS launchfeedback-x11 DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin-x11/effects/plugins)
