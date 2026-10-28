# Builds the game module's export table, so mods can call the engine directly by linking against soh.lib.
# libultraship's symbols are exported too: defined here when it is linked in statically, or forwarded to
# libultraship.dll when FORWARD_LIBULTRASHIP is set, so mods still import everything from soh. Runs at pre-link.

# Writes the C symbols the objects in `objects` define into `outputVariable`, one "name[ DATA]" per entry.
function(list_c_exports name objects outputVariable)
    list(SORT objects)
    set(objectList "")
    foreach(object IN LISTS objects)
        string(APPEND objectList "${object}\n")
    endforeach()

    set(objectListFile "${OBJECT_DIR}/${name}-objects.txt")
    set(rawExportsFile "${OBJECT_DIR}/${name}-exports.def")
    file(WRITE "${objectListFile}" "${objectList}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E __create_def "${rawExportsFile}" "${objectListFile}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "symbol export generation failed with ${result}")
    endif()

    file(STRINGS "${rawExportsFile}" lines)
    set(symbols "")
    # __create_def cuts the PCH marker __@@_PchSym_... at its first '@' and leaves a bare "__", which no object defines
    foreach(line IN LISTS lines)
        if(line MATCHES "^[ \t]+[A-Za-z_][A-Za-z0-9_]*([ \t]|$)" AND NOT line MATCHES "^[ \t]+__[ \t]")
            string(STRIP "${line}" symbol)
            list(APPEND symbols "${symbol}")
        endif()
    endforeach()
    set(${outputVariable} "${symbols}" PARENT_SCOPE)
    set(${outputVariable}_OBJECT_LIST "${objectListFile}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE objects "${OBJECT_DIR}/*.obj")
if(LIBULTRASHIP_OBJECT_DIR)
    file(GLOB_RECURSE libultrashipObjects "${LIBULTRASHIP_OBJECT_DIR}/*.obj")
    if(NOT FORWARD_LIBULTRASHIP)
        list(APPEND objects ${libultrashipObjects})
    endif()
endif()

list_c_exports(modapi "${objects}" symbols)
set(objectListFile "${symbols_OBJECT_LIST}")

# __create_def skips uninitialized C globals (COFF common symbols); the scan adds them as DATA exports.
if(PYTHON)
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/ListCommonSymbols.py" "${objectListFile}"
        OUTPUT_VARIABLE commonSymbolOutput
        RESULT_VARIABLE commonSymbolResult
    )
    if(NOT commonSymbolResult EQUAL 0)
        message(FATAL_ERROR "common symbol scan failed with ${commonSymbolResult}")
    endif()
    set(exportedNames "")
    foreach(symbol IN LISTS symbols)
        string(REGEX MATCH "^[A-Za-z_][A-Za-z0-9_]*" exportedName "${symbol}")
        list(APPEND exportedNames "${exportedName}")
    endforeach()
    string(REPLACE "\n" ";" commonSymbols "${commonSymbolOutput}")
    foreach(commonSymbol IN LISTS commonSymbols)
        if(commonSymbol AND NOT commonSymbol IN_LIST exportedNames)
            list(APPEND symbols "${commonSymbol} DATA")
        endif()
    endforeach()
endif()

if(FORWARD_LIBULTRASHIP AND libultrashipObjects)
    set(definedNames "")
    foreach(symbol IN LISTS symbols)
        string(REGEX MATCH "^[A-Za-z_][A-Za-z0-9_]*" definedName "${symbol}")
        list(APPEND definedNames "${definedName}")
    endforeach()
    list_c_exports(libultraship "${libultrashipObjects}" libultrashipSymbols)
    foreach(symbol IN LISTS libultrashipSymbols)
        string(REGEX MATCH "^[A-Za-z_][A-Za-z0-9_]*" forwardedName "${symbol}")
        if(forwardedName IN_LIST definedNames)
            continue()
        endif()
        set(forwarder "${forwardedName}=libultraship.${forwardedName}")
        if(symbol MATCHES "[ \t]DATA$")
            string(APPEND forwarder " DATA")
        endif()
        list(APPEND symbols "${forwarder}")
    endforeach()
endif()

list(REMOVE_DUPLICATES symbols)
list(SORT symbols)

list(LENGTH symbols symbolCount)
if(symbolCount EQUAL 0 OR symbolCount GREATER_EQUAL 65535)
    message(FATAL_ERROR "Invalid Windows export count: ${symbolCount}")
endif()

set(exports "EXPORTS\n")
foreach(symbol IN LISTS symbols)
    string(APPEND exports "    ${symbol}\n")
endforeach()

if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" previousExports)
endif()
if(NOT exports STREQUAL previousExports)
    file(WRITE "${OUTPUT}" "${exports}")
endif()
message(STATUS "ModAPI exports: ${symbolCount} C symbols")
