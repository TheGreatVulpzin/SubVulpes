# sv_add_plugin(TARGET ID MANIFEST path SOURCES file... [DEFINITIONS name...] [OFFICIAL])
#
# Builds a native plugin as a 32-bit DLL, puts it with its plugin.ini in build/plugins/ID, and adds the target
# TARGET-package. That target makes build/packages/ID-VERSION-native.svplugin with tools/sdk/pack.ps1.
# The version and the library name come from the manifest, so no value sits in two places.
# OFFICIAL marks a plugin of the server package. It must also appear in plugins/official.ini.
set(SV_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(SV_PLUGIN_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../src/plugins" CACHE PATH "The folder of the public sv_*.h headers")

function(sv_add_plugin target)
    cmake_parse_arguments(PLUGIN "OFFICIAL" "ID;MANIFEST" "SOURCES;DEFINITIONS" ${ARGN})
    if(NOT PLUGIN_ID OR NOT PLUGIN_MANIFEST OR NOT PLUGIN_SOURCES)
        message(FATAL_ERROR "sv_add_plugin needs ID, MANIFEST and SOURCES.")
    endif()
    file(STRINGS "${PLUGIN_MANIFEST}" manifest_lines REGEX "^(id|library)=")
    set(manifest_id "")
    set(manifest_library "")
    foreach(line IN LISTS manifest_lines)
        if(line MATCHES "^id=(.+)$")
            set(manifest_id "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^library=(.+)$")
            set(manifest_library "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT manifest_id STREQUAL PLUGIN_ID)
        message(FATAL_ERROR "The manifest ${PLUGIN_MANIFEST} does not declare id=${PLUGIN_ID}.")
    endif()
    if(NOT manifest_library MATCHES "^[A-Za-z0-9_.-]+\\.dll$")
        message(FATAL_ERROR "The manifest ${PLUGIN_MANIFEST} needs a library=NAME.dll line.")
    endif()
    get_filename_component(library_name "${manifest_library}" NAME_WE)
    set(plugin_dir "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_ID}")
    add_library(${target} MODULE ${PLUGIN_SOURCES})
    target_include_directories(${target} PRIVATE "${SV_PLUGIN_INCLUDE_DIR}")
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS ${PLUGIN_DEFINITIONS})
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    target_link_options(${target} PRIVATE /EXPORT:sv_plugin_entry)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${library_name}" PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${plugin_dir}" LIBRARY_OUTPUT_DIRECTORY "${plugin_dir}")
    file(MAKE_DIRECTORY "${plugin_dir}")
    configure_file("${PLUGIN_MANIFEST}" "${plugin_dir}/plugin.ini" COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PLUGIN_MANIFEST}")
    add_custom_target(${target}-package
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${SV_SDK_DIR}/pack.ps1"
            -Folder "${plugin_dir}" -OutputDir "${CMAKE_BINARY_DIR}/packages" -ServerExe "$<TARGET_FILE:subvulpes-server>"
        DEPENDS ${target} subvulpes-server VERBATIM)
    if(PLUGIN_OFFICIAL)
        set_property(GLOBAL APPEND PROPERTY SV_OFFICIAL_PLUGIN_IDS "${PLUGIN_ID}")
        set_property(GLOBAL APPEND PROPERTY SV_OFFICIAL_PLUGIN_TARGETS "${target}")
    endif()
endfunction()
