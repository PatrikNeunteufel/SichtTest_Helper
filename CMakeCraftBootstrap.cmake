# ==============================================================================
# CMakeCraftBootstrap.cmake — holt das Build-System in der gepinnten Version
# ==============================================================================
#
# Bezieht CMakeCraft gemaess cmakecraft.pin und laedt dessen Entry-Point.
# Diese Datei ist bewusst klein und stabil — sie ist der einzige committete
# Build-System-Code im Projekt. Aenderungen am Build-System passieren im
# CMakeCraft-Repo (SSOT), hier wird nur die Version gewechselt (cmakecraft.pin).
#
# Bezugsreihenfolge:
#   1. -DCMAKECRAFT_LOCAL_DIR=<pfad>  — Entwickler-Override: nutzt einen
#      CMakeCraft-Checkout DIREKT (Arbeitskopie, kein Klon). Fuer die
#      Build-System-Entwicklung gegen ein Projekt.
#   2. Cache: .externals/cmakecraft/<version>/ (bereits geholt -> offlinefaehig)
#   3. git clone --branch <version> von CMAKECRAFT_GIT_URL
#   4. git clone von CMAKECRAFT_FALLBACK_PATHS (lokale Repos, z. B. ../CMakeCraft)
#
# ==============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/cmakecraft.pin")

set(_craft_project_root "${CMAKE_CURRENT_LIST_DIR}")

# ------------------------------------------------------------------------------
# 1) Entwickler-Override
# ------------------------------------------------------------------------------
set(CMAKECRAFT_LOCAL_DIR "" CACHE PATH
    "Direkter CMakeCraft-Checkout (Override fuer Build-System-Entwicklung; leer = Pin verwenden)")

if(CMAKECRAFT_LOCAL_DIR)
    if(NOT IS_ABSOLUTE "${CMAKECRAFT_LOCAL_DIR}")
        get_filename_component(CMAKECRAFT_LOCAL_DIR
            "${_craft_project_root}/${CMAKECRAFT_LOCAL_DIR}" ABSOLUTE)
    endif()
    if(NOT EXISTS "${CMAKECRAFT_LOCAL_DIR}/CMakeCraft.cmake")
        message(FATAL_ERROR
            "[Bootstrap] CMAKECRAFT_LOCAL_DIR gesetzt, aber kein CMakeCraft.cmake in: ${CMAKECRAFT_LOCAL_DIR}")
    endif()
    message(STATUS "[Bootstrap] CMakeCraft (LOKAL-OVERRIDE): ${CMAKECRAFT_LOCAL_DIR}")
    include("${CMAKECRAFT_LOCAL_DIR}/CMakeCraft.cmake")
    return()
endif()

# ------------------------------------------------------------------------------
# 2) Cache in .externals/
# ------------------------------------------------------------------------------
set(_craft_dir "${_craft_project_root}/.externals/cmakecraft/${CMAKECRAFT_VERSION}")

if(NOT EXISTS "${_craft_dir}/CMakeCraft.cmake")

    find_package(Git QUIET)
    if(NOT GIT_EXECUTABLE)
        message(FATAL_ERROR "[Bootstrap] git nicht gefunden — wird zum Holen von CMakeCraft benoetigt.")
    endif()

    # 3) Primaerquelle, dann 4) Fallbacks
    set(_craft_sources "${CMAKECRAFT_GIT_URL}")
    foreach(_p IN LISTS CMAKECRAFT_FALLBACK_PATHS)
        if(NOT IS_ABSOLUTE "${_p}")
            get_filename_component(_p "${_craft_project_root}/${_p}" ABSOLUTE)
        endif()
        list(APPEND _craft_sources "${_p}")
    endforeach()

    set(_craft_ok FALSE)
    foreach(_src IN LISTS _craft_sources)
        message(STATUS "[Bootstrap] Hole CMakeCraft ${CMAKECRAFT_VERSION} von: ${_src}")
        file(REMOVE_RECURSE "${_craft_dir}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" clone --depth 1 --branch "${CMAKECRAFT_VERSION}"
                    --config advice.detachedHead=false "${_src}" "${_craft_dir}"
            RESULT_VARIABLE _craft_rc
            ERROR_VARIABLE _craft_err)
        if(_craft_rc EQUAL 0 AND EXISTS "${_craft_dir}/CMakeCraft.cmake")
            set(_craft_ok TRUE)
            break()
        endif()
        message(STATUS "[Bootstrap]   ... fehlgeschlagen (${_craft_rc})")
    endforeach()

    if(NOT _craft_ok)
        file(REMOVE_RECURSE "${_craft_dir}")
        message(FATAL_ERROR
            "[Bootstrap] CMakeCraft ${CMAKECRAFT_VERSION} konnte aus keiner Quelle geholt werden.\n"
            "  Quellen: ${_craft_sources}\n"
            "  Abhilfe:\n"
            "   - Netzverbindung herstellen (GitHub), oder\n"
            "   - lokalen CMakeCraft-Checkout bereitstellen (cmakecraft.pin: CMAKECRAFT_FALLBACK_PATHS), oder\n"
            "   - -DCMAKECRAFT_LOCAL_DIR=<pfad-zum-checkout> setzen.\n"
            "  Letzter Fehler:\n${_craft_err}")
    endif()

    message(STATUS "[Bootstrap] CMakeCraft ${CMAKECRAFT_VERSION} bereit: ${_craft_dir}")
else()
    message(STATUS "[Bootstrap] CMakeCraft ${CMAKECRAFT_VERSION} aus Cache: ${_craft_dir}")
endif()

include("${_craft_dir}/CMakeCraft.cmake")
