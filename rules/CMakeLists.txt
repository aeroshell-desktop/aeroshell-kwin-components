add_executable(aeroshell_update_default_rules update_default_rules.cpp)
target_link_libraries(aeroshell_update_default_rules
    KF6::ConfigCore
    Qt::Core
    Qt::DBus
)

install(FILES default_rules/aeroshell.rules DESTINATION ${CMAKE_INSTALL_PREFIX}/share/aeroshell/default_rules/)
install(TARGETS aeroshell_update_default_rules DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
