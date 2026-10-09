# ==============================================================================
# Source.cmake for library src/
# CMake Architecture V2 - Source Collection
# ==============================================================================
# Location: projects/libs/SichttestSteuerung/src/Source.cmake
# Target:   SichttestSteuerung — DLL zur Steuerung der geprüften Anwendung (ohne Qt)
# ==============================================================================

dbg(${DBG_OFTEN}
    "${CMAKE_CURRENT_LIST_DIR}/Source.cmake
          =============================================\n" ID INCLUDE_MSG)

# ==============================================================================
# Local file lists for THIS directory
# ==============================================================================

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/Json.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Kanal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Sitzung.cpp"
)

set(_local_headers
    "${CMAKE_CURRENT_LIST_DIR}/Json.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/Kanal.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/../include/sichttest_steuerung.h"
)

set(_local_includes
    "${CMAKE_CURRENT_LIST_DIR}"
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
