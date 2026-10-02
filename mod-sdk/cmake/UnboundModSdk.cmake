include_guard(GLOBAL)

get_filename_component(UNBOUND_MOD_SDK_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT UNBOUND_ROOT)
    get_filename_component(UNBOUND_ROOT "${UNBOUND_MOD_SDK_DIRECTORY}/.." ABSOLUTE)
endif()
set(UNBOUND_ROOT "${UNBOUND_ROOT}" CACHE PATH "Unbound source checkout the mods compile against")
set(UNBOUND_IMPORT_LIBRARY "${UNBOUND_ROOT}/x64/$<CONFIG>/soh.lib" CACHE STRING
    "Windows only: soh.lib, the import library of the soh.exe the mods will load into")
set(UNBOUND_GAME_DIR "" CACHE PATH "Game folder; when set, every built mod is copied into its mods/ folder")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(UNBOUND_MOD_PLATFORM windows_x64)
    else()
        set(UNBOUND_MOD_PLATFORM windows_x86)
    endif()
elseif(APPLE)
    set(UNBOUND_MOD_PLATFORM darwin)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(UNBOUND_MOD_PLATFORM linux_x64)
else()
    message(FATAL_ERROR "Unsupported native mod target: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(NOT TARGET unbound_mod_sdk)
    add_library(unbound_mod_sdk INTERFACE)
    target_include_directories(unbound_mod_sdk INTERFACE
        "${UNBOUND_ROOT}/soh"
        "${UNBOUND_ROOT}/soh/include"
        "${UNBOUND_ROOT}/soh/src"
        "${UNBOUND_ROOT}/soh/assets"
        "${UNBOUND_ROOT}/libultraship/include"
    )
    target_compile_definitions(unbound_mod_sdk INTERFACE
        F3DEX_GBI_2
        NOMINMAX
        UNBOUND_MOD
        GBI_S32_VTX=1
        GBI_FLOAT_MTX=1
        CONTROLLERBUTTONS_T=uint32_t
    )
    target_compile_features(unbound_mod_sdk INTERFACE c_std_11 cxx_std_20)
    if(MSVC)
        target_compile_options(unbound_mod_sdk INTERFACE $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor> /permissive- /bigobj)
    else()
        target_compile_options(unbound_mod_sdk INTERFACE
            $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
            $<$<COMPILE_LANGUAGE:C>:-Wno-int-conversion>
            $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-function-pointer-types>
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
        )
    endif()
    if(WIN32)
        target_link_libraries(unbound_mod_sdk INTERFACE "${UNBOUND_IMPORT_LIBRARY}")
    elseif(APPLE)
        target_link_options(unbound_mod_sdk INTERFACE "LINKER:-undefined,dynamic_lookup")
    endif()

    foreach(helper z64items z64aiming z64wheel)
        add_library(${helper} INTERFACE)
        target_link_libraries(${helper} INTERFACE unbound_mod_sdk)
        target_include_directories(${helper} INTERFACE "${UNBOUND_MOD_SDK_DIRECTORY}/include/${helper}")
    endforeach()
endif()

function(unbound_add_mod name)
    cmake_parse_arguments(MOD "" "DIRECTORY" "SOURCES;LIBRARIES" ${ARGN})
    if(NOT MOD_DIRECTORY OR NOT MOD_SOURCES)
        message(FATAL_ERROR "unbound_add_mod(${name}) requires DIRECTORY and SOURCES")
    endif()
    if(NOT EXISTS "${MOD_DIRECTORY}/manifest.json")
        message(FATAL_ERROR "unbound_add_mod(${name}): ${MOD_DIRECTORY}/manifest.json is missing")
    endif()

    set(package "${CMAKE_BINARY_DIR}/packages/$<CONFIG>/${name}.o2r")
    add_library(${name} MODULE ${MOD_SOURCES})
    target_link_libraries(${name} PRIVATE unbound_mod_sdk ${MOD_LIBRARIES})
    set_target_properties(${name} PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND Python3::Interpreter "${UNBOUND_MOD_SDK_DIRECTORY}/tools/pack_mod.py"
            --mod-directory "${MOD_DIRECTORY}"
            --binary "$<TARGET_FILE:${name}>"
            --platform "${UNBOUND_MOD_PLATFORM}"
            --output "${package}"
        VERBATIM
    )
    if(UNBOUND_GAME_DIR)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${UNBOUND_GAME_DIR}/mods"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${package}" "${UNBOUND_GAME_DIR}/mods/${name}.o2r"
            VERBATIM
        )
    endif()
endfunction()

function(unbound_add_mod_directory directory)
    get_filename_component(name "${directory}" NAME)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${directory}/*.c" "${directory}/*.cpp")
    list(FILTER sources EXCLUDE REGEX "/assets/")
    list(FILTER sources EXCLUDE REGEX "\\.inc\\.c$")
    if(NOT sources)
        message(FATAL_ERROR "Mod ${name} in ${directory} has no .c or .cpp sources")
    endif()
    unbound_add_mod(${name} DIRECTORY "${directory}" SOURCES ${sources} LIBRARIES z64items z64aiming z64wheel)
endfunction()

function(unbound_add_mods_in parent)
    file(GLOB children LIST_DIRECTORIES true "${parent}/*")
    foreach(child IN LISTS children)
        if(EXISTS "${child}/CMakeLists.txt")
            add_subdirectory("${child}")
        elseif(EXISTS "${child}/manifest.json")
            unbound_add_mod_directory("${child}")
        endif()
    endforeach()
endfunction()
