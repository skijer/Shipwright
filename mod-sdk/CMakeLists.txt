cmake_minimum_required(VERSION 3.24)
project(UnboundMods LANGUAGES C CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/UnboundModSdk.cmake")

option(UNBOUND_BUILD_TEMPLATES "Build the SDK templates next to the mods in mods/" ON)

if(UNBOUND_BUILD_TEMPLATES)
    unbound_add_mods_in("${CMAKE_CURRENT_SOURCE_DIR}/templates")
endif()
unbound_add_mods_in("${CMAKE_CURRENT_SOURCE_DIR}/mods")
