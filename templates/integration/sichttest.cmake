# ==============================================================================
# sichttest.cmake — template: consume the Sichttest package WITHOUT CMakeCraft
# ==============================================================================
# Needs CMake 3.26 or later and an unchanged copy of CMakeCraftPackage.cmake
# (standalone file from the CMakeCraft repository, root folder) in your repo,
# here assumed under cmake/. The version is in sichttest.pin
# (templates/sichttest.pin).
#
# Include this file after add_executable(MyApp ...), for example with
#   include(cmake/sichttest.cmake)
#
# If the package can neither be found in the cache nor fetched, configure warns,
# SICHTTEST_VORHANDEN stays undefined and the application builds without
# control. For development, -DSICHTTEST_LOCAL_DIR=<unpacked package> replaces
# the pin.
# ==============================================================================

include("${CMAKE_SOURCE_DIR}/sichttest.pin")
include("${CMAKE_SOURCE_DIR}/cmake/CMakeCraftPackage.cmake")

craft_package_fetch(
    NAME      sichttest
    PIN_FILE  "${CMAKE_SOURCE_DIR}/sichttest.pin"
    CACHE_DIR "${CMAKE_SOURCE_DIR}/.externals/sichttest/${SICHTTEST_VERSION}"   # keep out of git
    OUT_ROOT  MYAPP_SICHTTEST_ROOT)

craft_package_deploy(
    TARGET        MyApp
    ROOT          "${MYAPP_SICHTTEST_ROOT}"
    NAME          sichttest
    INCLUDE_DIRS  include                          # PRIVATE to MyApp
    DEFINE        SICHTTEST_VORHANDEN              # set only when the package is there
    RUNTIME_FILES bin/SichttestSteuerung1.dll      # next to MyApp.exe
    RUNTIME_DIRS  sichttest)                       # the tester with its own Qt, as subfolder

if(MYAPP_SICHTTEST_ROOT)
    message(STATUS "Sichttest: ON (${SICHTTEST_VERSION}: ${MYAPP_SICHTTEST_ROOT})")
else()
    message(STATUS "Sichttest: OFF (package not available, building without control)")
endif()
