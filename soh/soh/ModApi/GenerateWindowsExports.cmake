file(GLOB_RECURSE objects "${OBJECT_DIR}/*.obj")
if(LIBULTRASHIP_OBJECT_DIR)
    file(GLOB_RECURSE libultrashipObjects "${LIBULTRASHIP_OBJECT_DIR}/*.obj")
    list(APPEND objects ${libultrashipObjects})
endif()
list(SORT objects)

set(objectList "")
foreach(object IN LISTS objects)
    string(APPEND objectList "${object}\n")
endforeach()

set(objectListFile "${OBJECT_DIR}/modapi-objects.txt")
set(rawExportsFile "${OBJECT_DIR}/modapi-exports.def")
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

foreach(line IN LISTS lines)
    if(line MATCHES "^[ \t]+[A-Za-z_][A-Za-z0-9_]*([ \t]|$)")
        string(STRIP "${line}" symbol)
        list(APPEND symbols "${symbol}")
    endif()
endforeach()

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
