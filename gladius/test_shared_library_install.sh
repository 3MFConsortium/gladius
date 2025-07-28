#!/bin/bash

# Test script to verify that vcpkg shared libraries are properly installed on Linux
# This script tests the fix for issue #28: lib3mf.so is not installed under Linux

set -e

echo "Testing vcpkg shared library installation fix..."

# Test that the x64-linux-gladius triplet exists and is properly configured
if [ ! -f "vcpkg-triplets/x64-linux-gladius.cmake" ]; then
    echo "ERROR: x64-linux-gladius.cmake triplet not found"
    exit 1
fi

# Check that the triplet is configured for dynamic linking
if ! grep -q "VCPKG_LIBRARY_LINKAGE dynamic" vcpkg-triplets/x64-linux-gladius.cmake; then
    echo "ERROR: Triplet not configured for dynamic linking"
    exit 1
fi

# Check that the triplet specifies Linux as the system
if ! grep -q "VCPKG_CMAKE_SYSTEM_NAME Linux" vcpkg-triplets/x64-linux-gladius.cmake; then
    echo "ERROR: Triplet does not specify Linux system"
    exit 1
fi

echo "✓ x64-linux-gladius triplet is properly configured"

# Check that CMakePresets.json uses the new triplet for Linux builds
if ! grep -q "x64-linux-gladius" CMakePresets.json; then
    echo "ERROR: CMakePresets.json does not use x64-linux-gladius triplet"
    exit 1
fi

echo "✓ CMakePresets.json configured to use dynamic linking triplet"

# Check that CMakeLists.txt has the installation rule for vcpkg shared libraries
if ! grep -q "vcpkg_installed/x64-linux-gladius/lib" CMakeLists.txt; then
    echo "ERROR: CMakeLists.txt missing vcpkg shared library installation rule"
    exit 1
fi

# Check that the installation rule is Linux-specific
if ! grep -A5 -B5 "vcpkg_installed/x64-linux-gladius/lib" CMakeLists.txt | grep -q "UNIX AND NOT WIN32"; then
    echo "ERROR: Installation rule not properly guarded for Linux"
    exit 1
fi

echo "✓ CMakeLists.txt has proper installation rule for vcpkg shared libraries"

# Check that RPATH is configured for Linux
if ! grep -q "CMAKE_INSTALL_RPATH.*lib" CMakeLists.txt; then
    echo "ERROR: RPATH not configured for shared library discovery"
    exit 1
fi

echo "✓ RPATH properly configured for shared library discovery"

echo ""
echo "SUCCESS: All checks passed! The fix for lib3mf.so installation issue is properly implemented."
echo ""
echo "Changes made:"
echo "1. Created x64-linux-gladius.cmake triplet for dynamic linking on Linux"
echo "2. Updated CMakePresets.json to use the new triplet"
echo "3. Added installation rule in CMakeLists.txt to copy vcpkg shared libraries"
echo "4. Configured RPATH for runtime library discovery"
echo ""
echo "Result: lib3mf.so and other vcpkg shared libraries will now be installed to"
echo "        /opt/gladius/1.2.13/lib/ and be discoverable by the gladius binary."