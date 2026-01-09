#######################################
# Effect

set(aeroglide_SOURCES
    glide.cpp
    main.cpp
)

kconfig_add_kcfg_files(aeroglide_SOURCES
    glideconfig.kcfgc
)

add_library(aeroglide MODULE ${aeroglide_SOURCES})
add_library(aeroglide-x11 MODULE ${aeroglide_SOURCES})

#kwin_add_builtin_effect(glide ${glide_SOURCES})
target_link_libraries(aeroglide PRIVATE
    KF6::CoreAddons
    KF6::ConfigGui
    KWin::kwin
)
target_link_libraries(aeroglide-x11 PRIVATE
    KF6::CoreAddons
    KF6::ConfigGui
    KWinX11::kwin
)

#######################################
# Config
set(kwin_aeroglide_config_SRCS glide_config.cpp)
ki18n_wrap_ui(kwin_aeroglide_config_SRCS glide_config.ui)
kconfig_add_kcfg_files(kwin_aeroglide_config_SRCS glideconfig.kcfgc)
qt_add_dbus_interface(kwin_aeroglide_config_SRCS ${KWIN_EFFECTS_INTERFACE} kwineffects_interface)

#kwin_add_effect_config(kwin_aeroglide_config ${kwin_aeroglide_config_SRCS})

add_library(kwin_aeroglide_config MODULE ${kwin_aeroglide_config_SRCS})
target_link_libraries(kwin_aeroglide_config
    KF6::KCMUtils
    KF6::CoreAddons
    KF6::I18n
    Qt6::DBus
    KF6::WindowSystem
    #KWinEffectsInterface
)

install(TARGETS aeroglide DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins)
set_target_properties(aeroglide-x11 PROPERTIES OUTPUT_NAME x11/aeroglide)
install(TARGETS aeroglide-x11 DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin-x11/effects/plugins)
install(TARGETS kwin_aeroglide_config DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs)
install(TARGETS kwin_aeroglide_config DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin-x11/effects/configs)
