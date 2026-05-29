if(KWIN_BUILD_WAYLAND)
    add_subdirectory(wayland)
else()
    add_subdirectory(x11)
endif()


