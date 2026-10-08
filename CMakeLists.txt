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
# ==============================================================================

cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/CMakeCraftBootstrap.cmake")
