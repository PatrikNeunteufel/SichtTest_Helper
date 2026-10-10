# ==============================================================================
# Source.cmake for executable src/
# CMake Architecture V2 - Source Collection
# ==============================================================================
# Location: projects/exec/Sichttest/src/Source.cmake
# Target:   Sichttest — Fenster zum Abhaken einer Sichttest-Liste (Session 93)
# ==============================================================================

dbg(${DBG_OFTEN}
    "${CMAKE_CURRENT_LIST_DIR}/Source.cmake
          =============================================\n" ID INCLUDE_MSG)

# ==============================================================================
# Local file lists for THIS directory
# ==============================================================================

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Ergebnis.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Indikator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Kanal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Projekt.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Protokoll.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Steuerung.cpp"
)

set(_local_headers
    "${CMAKE_CURRENT_LIST_DIR}/Ergebnis.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/Indikator.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/Kanal.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/Projekt.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/Protokoll.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/Steuerung.hpp"
)

# Nur der Kopf der DLL (Fassungen von S und P): der Tester lädt die DLL nie.
set(_local_includes
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_SOURCE_DIR}/projects/libs/SichttestSteuerung/include"
)

set(_local_templates
)

set(_local_inlines
)

set(_local_impl
)

# ==============================================================================
# Aggregate to TARGET variables
# ==============================================================================

list(APPEND ${TARGET_NAME}_SOURCES   ${_local_sources})
list(APPEND ${TARGET_NAME}_HEADERS   ${_local_headers})
list(APPEND ${TARGET_NAME}_TEMPLATES ${_local_templates})
list(APPEND ${TARGET_NAME}_INLINES   ${_local_inlines})
list(APPEND ${TARGET_NAME}_IMPL      ${_local_impl})
list(APPEND ${TARGET_NAME}_INCLUDES  ${_local_includes})

# ==============================================================================
# Cleanup local variables
# ==============================================================================

unset(_local_sources)
unset(_local_headers)
unset(_local_templates)
unset(_local_inlines)
unset(_local_impl)
unset(_local_includes)
