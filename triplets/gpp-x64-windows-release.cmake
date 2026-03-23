set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)

# Keep this package variant aligned with the consumer-side
# SPDLOG_WCHAR_FILENAMES define already present in the .vcxproj files.
set(SPDLOG_WCHAR_FILENAMES ON)
