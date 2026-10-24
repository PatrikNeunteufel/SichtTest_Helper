# ==============================================================================
# CMakeLists.txt – SichtTest_Helper (Konsument von CMakeCraft)
# ==============================================================================
#
# Das Build-System (CMakeCraft) liegt nicht im Repo: Es wird beim Configure in
# der gepinnten Version (cmakecraft.pin) nach .externals/ geholt. Was gebaut
# wird, definiert ausschliesslich Solution.json.
#
#   Version wechseln:            cmakecraft.pin editieren
#   Build-System entwickeln:     -DCMAKECRAFT_LOCAL_DIR=../CMakeCraft
#
# Die Zeile project(... LANGUAGES NONE) ist Pflicht (seit CMakeCraft v0.13.0,
# sonst E005): Ohne sie setzt CMake ein eigenes project() VOR
# cmake_minimum_required() und richtet die Compiler nach alten Regeln ein.
# Name und Version kommen weiter aus Solution.json.
#
# ==============================================================================

cmake_minimum_required(VERSION 3.26)
project(SichtTest_Helper LANGUAGES NONE)

include("${CMAKE_CURRENT_LIST_DIR}/CMakeCraftBootstrap.cmake")
