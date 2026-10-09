# ==============================================================================
# Source.cmake for executable src/
# CMake Architecture V2 - Source Collection
# ==============================================================================
# Location: projects/exec/Gegenprobe/src/Source.cmake
# Target:   Gegenprobe — Anwendung ohne Qt, lädt die DLL zur Laufzeit (Selbsttest)
# ==============================================================================

dbg(${DBG_OFTEN}
    "${CMAKE_CURRENT_LIST_DIR}/Source.cmake
          =============================================\n" ID INCLUDE_MSG)

# ==============================================================================
# Local file lists for THIS directory
# ==============================================================================

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)

set(_local_headers
)

# Nur der Kopf: die Gegenprobe linkt nicht gegen die DLL, sie lädt sie zur Laufzeit.
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
