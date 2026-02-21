# AeroShell KWin Components 

This repository contains AeroShell components related to KWin. It contains the following plugins:

- JS Effects (dimscreen, fadingpopups, etc.)
- C++ Effects (aeroglassblur, aeroglide, etc.)
- Alt+Tab Task Switchers (thumbnails, flip3d)
- Scripts (SMOD Peek)
- Outline for window snapping

These KWin components have been separated out from the main repository so that the code can be shared between ATP and projects like VistaThemePlasma. 

## Building

```bash
cmake -B build -G Ninja -DKWIN_BUILD_WAYLAND=ON -DCMAKE_INSTALL_PREFIX=/usr . # Use Ninja for faster builds 
cmake --build build
sudo cmake --install build
# Alternatively, for testing purposes
DESTDIR=output cmake --install build
```

Options:

- `KWIN_BUILD_WAYLAND` - Build effects for Wayland instead of X11. Off by default.
- `KWIN_INSTALL_MISC` - Install other non-C++ components. On by default.

### Note for Wayland Users
You should define `KWIN_BUILD_WAYLAND` before building, otherwise you will get build errors.

```bash
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DKWIN_BUILD_WAYLAND=ON .
cmake --build build
sudo cmake --install build
```
