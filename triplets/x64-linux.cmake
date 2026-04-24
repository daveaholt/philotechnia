set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Disable Vivante EGL - vendor SDK not present on standard x86_64 Linux.
# Without this, Qt's feature detection tries to compile against
# EGL/eglvivante.h which does not exist on Ubuntu runners.
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS -DFEATURE_egl_viv=OFF)
