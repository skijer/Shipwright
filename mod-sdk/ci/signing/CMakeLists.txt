cmake_minimum_required(VERSION 3.24)
project(UnboundSignedMods LANGUAGES C CXX)

set(UNBOUND_SIGNED_MODS_DIR "" CACHE PATH "Folder whose subfolders are the mods submitted for signing")
if(NOT IS_DIRECTORY "${UNBOUND_SIGNED_MODS_DIR}")
    message(FATAL_ERROR "UNBOUND_SIGNED_MODS_DIR must name the submitted mods folder")
endif()

include("${UNBOUND_ROOT}/mod-sdk/cmake/UnboundModSdk.cmake")

# Never add_subdirectory: a submitted mod must not run CMake code of its own while it is built for signing.
file(GLOB children LIST_DIRECTORIES true "${UNBOUND_SIGNED_MODS_DIR}/*")
foreach(child IN LISTS children)
    if(EXISTS "${child}/manifest.json")
        unbound_add_mod_directory("${child}")
    endif()
endforeach()
