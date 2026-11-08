cmake_minimum_required(VERSION 3.20)
project(SubVulpes LANGUAGES C)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "SubVulpes needs MSVC x86 (32-bit). Use tools/build_hook.bat.")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

file(GLOB SV_GAME_SOURCES CONFIGURE_DEPENDS src/common/*.c src/net/*.c src/game/*.c src/server/*.c src/plugins/*.c)
list(APPEND SV_GAME_SOURCES src/platform/os_calls.c)
add_executable(subvulpes-server
    src/standalone/main.c src/platform/x87_math.c
    src/platform/plat_imports.c ${SV_GAME_SOURCES})
target_include_directories(subvulpes-server PRIVATE
    src/include src/platform src/common src/net src/game src/plugins)
target_compile_definitions(subvulpes-server PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
set(SV_SERVER_VERSION "" CACHE STRING "The version that the server reports and compares with server_min (empty for a development build)")
if(SV_SERVER_VERSION)
    target_compile_definitions(subvulpes-server PRIVATE SV_SERVER_VERSION="${SV_SERVER_VERSION}")
endif()
target_compile_options(subvulpes-server PRIVATE /W4 /WX /utf-8)
set_source_files_properties(${SV_GAME_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:IA32;/Z7")
set_source_files_properties(src/game/ai_car.c
    PROPERTIES COMPILE_OPTIONS "/arch:IA32;/W0;/WX-;/we4013;/Z7")

set(SV_STORAGE_HEADERS)
foreach(header sr_types.h sr_runtime.h sr_storage.h sr_storage_types.h sr_limits.h)
    list(APPEND SV_STORAGE_HEADERS "${CMAKE_SOURCE_DIR}/src/include/${header}")
endforeach()
set_property(SOURCE ${SV_GAME_SOURCES} src/standalone/main.c src/platform/x87_math.c
    APPEND PROPERTY OBJECT_DEPENDS "${SV_STORAGE_HEADERS}")
file(GLOB SV_PLUGIN_HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/plugins/*.h")
set_property(SOURCE ${SV_GAME_SOURCES} src/standalone/main.c plugins/status/status.c
    tests/plugin_fixture.c tests/plugin_api_fixture.c tests/plugin_host_test.c tests/plugin_game_fixture.c tests/plugin_game_test.c tests/plugin_runtime_fixture.c
    APPEND PROPERTY OBJECT_DEPENDS "${SV_PLUGIN_HEADERS}")
set_property(SOURCE src/game/human_body.c src/game/human_arms.c src/game/human_pose.c
    src/game/humans.c src/game/item_physics.c src/game/objects.c src/game/wheels.c tests/physics_bounds.c
    APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/game/bonds.h;${CMAKE_SOURCE_DIR}/src/game/objects.h")
target_link_options(subvulpes-server PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
set_target_properties(subvulpes-server PROPERTIES OUTPUT_NAME subvulpes_server)

include(tools/sdk/sv_plugin.cmake)
sv_add_plugin(sv-status ID sv.status MANIFEST "${CMAKE_SOURCE_DIR}/plugins/status/plugin.ini"
    SOURCES plugins/status/status.c OFFICIAL)
sv_add_plugin(sv-permissions ID sv.permissions MANIFEST "${CMAKE_SOURCE_DIR}/plugins/permissions/plugin.ini"
    SOURCES plugins/permissions/permissions.c plugins/permissions/groups.c plugins/common/official.c OFFICIAL)
sv_add_plugin(sv-admcmd ID sv.admcmd MANIFEST "${CMAKE_SOURCE_DIR}/plugins/admcmd/plugin.ini"
    SOURCES plugins/admcmd/admcmd.c plugins/common/official.c plugins/common/permissions.c OFFICIAL)
set_property(SOURCE plugins/permissions/permissions.c plugins/permissions/groups.c plugins/common/official.c
    tests/official/permission_groups_test.c tests/official/permission_probe.c APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/plugins/permissions/sv_permissions.h;${CMAKE_SOURCE_DIR}/plugins/permissions/groups.h;${CMAKE_SOURCE_DIR}/plugins/common/official.h")
set_property(SOURCE plugins/admcmd/admcmd.c plugins/common/permissions.c plugins/common/official.c
    plugins/permissions/permissions.c tests/official/admcmd_probe.c tests/official/admcmd_unit_test.c
    APPEND PROPERTY OBJECT_DEPENDS
    "${SV_PLUGIN_HEADERS};${CMAKE_SOURCE_DIR}/plugins/common/permissions.h;${CMAKE_SOURCE_DIR}/plugins/common/official.h;${CMAKE_SOURCE_DIR}/plugins/permissions/sv_permissions.h")
set_property(SOURCE tests/official/admcmd_unit_test.c APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/plugins/admcmd/admcmd.c")
# Package inspection uses the installer shipped beside the server.
configure_file(tools/plugins/install_plugin.ps1 "${CMAKE_BINARY_DIR}/install_plugin.ps1" COPYONLY)
configure_file(plugins/plugins.ini "${CMAKE_BINARY_DIR}/plugins/plugins.ini" COPYONLY)
# The official list, the declared official plugins and the default profile must agree.
get_property(SV_OFFICIAL_IDS GLOBAL PROPERTY SV_OFFICIAL_PLUGIN_IDS)
get_property(SV_OFFICIAL_TARGETS GLOBAL PROPERTY SV_OFFICIAL_PLUGIN_TARGETS)
file(STRINGS plugins/official.ini SV_OFFICIAL_LINES REGEX "^[a-z0-9][a-z0-9_.-]*=(enabled|disabled|required)$")
set(SV_OFFICIAL_LISTED "")
foreach(line IN LISTS SV_OFFICIAL_LINES)
    string(REGEX REPLACE "=.*$" "" listed_id "${line}")
    list(APPEND SV_OFFICIAL_LISTED "${listed_id}")
endforeach()
list(SORT SV_OFFICIAL_LISTED)
list(SORT SV_OFFICIAL_IDS)
if(NOT "${SV_OFFICIAL_LISTED}" STREQUAL "${SV_OFFICIAL_IDS}")
    message(FATAL_ERROR "plugins/official.ini lists [${SV_OFFICIAL_LISTED}] but CMake declares [${SV_OFFICIAL_IDS}] as official.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/plugins/official.ini")
add_custom_target(sv-official-approvals ALL
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tools/plugins/write_official_approvals.ps1"
        -Official "${CMAKE_SOURCE_DIR}/plugins/official.ini" -PluginsDir "${CMAKE_BINARY_DIR}/plugins"
        -Output "${CMAKE_BINARY_DIR}/plugins/native-approvals.ini"
    VERBATIM)
add_dependencies(sv-official-approvals ${SV_OFFICIAL_TARGETS})

# The SDK package: the public headers, a template plugin, the tools and the pages of the API.
set(SV_SDK_VERSION "v0.0.0-dev" CACHE STRING "The version of the SDK package")

# The test room is a tool for developers. The server package does not carry it. It is the example of a plugin built with the SDK.
sv_add_plugin(sv-testroom ID sv.testroom MANIFEST "${CMAKE_SOURCE_DIR}/plugins/testroom/plugin.ini"
    SOURCES plugins/testroom/testroom.c)

# Optional SDK example. The official profile and server package do not carry it.
add_subdirectory(plugins/jukebox)

set(SV_LAUNCHER "${CMAKE_SOURCE_DIR}/build/launcher/subvulpes_launcher.exe")
add_custom_command(OUTPUT "${SV_LAUNCHER}"
    COMMAND cmd /c "${CMAKE_SOURCE_DIR}/tools/launcher/build.bat"
    DEPENDS tools/launcher/Launcher.cs tools/launcher/LauncherUi.cs tools/launcher/LauncherUiMore.cs tools/launcher/LauncherServer.cs tools/launcher/LauncherSettings.cs tools/launcher/LauncherPlugins.cs tools/launcher/LauncherHelp.cs tools/launcher/LauncherStore.cs tools/launcher/help.md tools/launcher/index-public.pem tools/release/version-names.csv tools/launcher/build.bat tools/release/default-config.ini
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" VERBATIM)
add_custom_target(subvulpes-launcher ALL DEPENDS "${SV_LAUNCHER}")

include(CTest)
if(BUILD_TESTING)
    add_library(sv-status-update MODULE plugins/status/status.c)
    target_include_directories(sv-status-update PRIVATE src/plugins)
    target_compile_definitions(sv-status-update PRIVATE _CRT_SECURE_NO_WARNINGS SV_STATUS_VERSION="0.1.1")
    target_compile_options(sv-status-update PRIVATE /W4 /WX /utf-8)
    target_link_options(sv-status-update PRIVATE /EXPORT:sv_plugin_entry)
    set_target_properties(sv-status-update PROPERTIES PREFIX "" OUTPUT_NAME status-update
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_library(sv-plugin-fixture MODULE tests/plugin_fixture.c)
    target_include_directories(sv-plugin-fixture PRIVATE src/plugins)
    target_compile_definitions(sv-plugin-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-plugin-fixture PRIVATE /W4 /WX /utf-8)
    target_link_options(sv-plugin-fixture PRIVATE /EXPORT:sv_plugin_entry)
    set_target_properties(sv-plugin-fixture PROPERTIES PREFIX "" OUTPUT_NAME fixture
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    # One source makes a library plugin and a plugin that uses its API.
    foreach(role library consumer)
        add_library(sv-plugin-${role} MODULE tests/plugin_api_fixture.c)
        target_include_directories(sv-plugin-${role} PRIVATE src/plugins)
        target_compile_definitions(sv-plugin-${role} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(sv-plugin-${role} PRIVATE /W4 /WX /utf-8)
        target_link_options(sv-plugin-${role} PRIVATE /EXPORT:sv_plugin_entry)
        set_target_properties(sv-plugin-${role} PROPERTIES PREFIX "" OUTPUT_NAME ${role}
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endforeach()
    target_compile_definitions(sv-plugin-library PRIVATE FIXTURE_LIBRARY)
    add_library(sv-plugin-helper SHARED tests/plugin_dependency_helper.c)
    set_target_properties(sv-plugin-helper PROPERTIES PREFIX "" OUTPUT_NAME sv_test_helper_0e9b6
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_library(sv-plugin-dependent MODULE tests/plugin_dependency_fixture.c)
    target_include_directories(sv-plugin-dependent PRIVATE src/plugins)
    target_compile_definitions(sv-plugin-dependent PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-plugin-dependent PRIVATE /W4 /WX /utf-8)
    target_link_libraries(sv-plugin-dependent PRIVATE sv-plugin-helper)
    target_link_options(sv-plugin-dependent PRIVATE /EXPORT:sv_plugin_entry)
    set_target_properties(sv-plugin-dependent PROPERTIES PREFIX "" OUTPUT_NAME dependent
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_library(sv-trust-fixture MODULE tests/runner/trust_fixture.c)
    target_include_directories(sv-trust-fixture PRIVATE src/plugins)
    target_compile_definitions(sv-trust-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-trust-fixture PRIVATE /W4 /WX /utf-8)
    target_link_options(sv-trust-fixture PRIVATE /EXPORT:sv_plugin_entry)
    set_target_properties(sv-trust-fixture PROPERTIES PREFIX "" OUTPUT_NAME trust-fixture
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_executable(sv-plugin-test tests/plugin_host_test.c src/plugins/plugin_host.c
        src/plugins/plugin_files.c src/plugins/plugin_control.c src/plugins/plugin_events.c src/plugins/plugin_config.c
        src/plugins/plugin_manifest.c)
    target_include_directories(sv-plugin-test PRIVATE src/plugins)
    target_compile_definitions(sv-plugin-test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-plugin-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(sv-plugin-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_executable(sv-capability-test tests/sdk_caps_test.c src/plugins/plugin_manifest.c
        src/plugins/plugin_config.c src/plugins/plugin_files.c)
    target_include_directories(sv-capability-test PRIVATE src/plugins)
    target_compile_definitions(sv-capability-test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-capability-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(sv-capability-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME capability-bits COMMAND sv-capability-test)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_executable(permission-groups-test tests/official/permission_groups_test.c plugins/permissions/groups.c plugins/common/official.c)
    target_include_directories(permission-groups-test PRIVATE src/plugins plugins/permissions)
    target_compile_definitions(permission-groups-test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(permission-groups-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(permission-groups-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME permissions-groups COMMAND permission-groups-test)
    set_tests_properties(permissions-groups PROPERTIES TIMEOUT 10)
    sv_add_plugin(sv-permission-probe ID test.permissions
        MANIFEST "${CMAKE_SOURCE_DIR}/tests/official/permission_probe/plugin.ini"
        SOURCES tests/official/permission_probe.c plugins/common/official.c)
    add_test(NAME permissions-sdk COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/official/permissions_test.py" "${CMAKE_BINARY_DIR}")
    sv_add_plugin(sv-admcmd-probe ID test.admcmd MANIFEST "${CMAKE_SOURCE_DIR}/tests/official/admcmd_probe/plugin.ini"
        SOURCES tests/official/admcmd_probe.c plugins/common/official.c)
    add_test(NAME admcmd-sdk COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/official/admcmd_test.py" "${CMAKE_BINARY_DIR}")
    add_executable(admcmd-unit-test tests/official/admcmd_unit_test.c plugins/common/official.c plugins/common/permissions.c)
    target_include_directories(admcmd-unit-test PRIVATE src/plugins)
    target_compile_definitions(admcmd-unit-test PRIVATE _CRT_SECURE_NO_WARNINGS
        SV_OFFICIAL_ID="sv.admcmd" SV_OFFICIAL_VERSION="0.1.0")
    target_compile_options(admcmd-unit-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(admcmd-unit-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME admcmd-contracts COMMAND admcmd-unit-test)
    set_tests_properties(admcmd-contracts PROPERTIES TIMEOUT 10)
    add_executable(jukebox-instruments-test tests/jukebox_instruments_test.c plugins/jukebox/instruments.c)
    target_include_directories(jukebox-instruments-test PRIVATE plugins/jukebox)
    target_compile_definitions(jukebox-instruments-test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(jukebox-instruments-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(jukebox-instruments-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME jukebox-instruments COMMAND jukebox-instruments-test "${CMAKE_SOURCE_DIR}/plugins/jukebox/instruments.ini")
    add_executable(jukebox-host-test tests/jukebox_host_test.c)
    target_include_directories(jukebox-host-test PRIVATE src/plugins)
    target_compile_definitions(jukebox-host-test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(jukebox-host-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(jukebox-host-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_dependencies(jukebox-host-test sv-jukebox)
    add_test(NAME jukebox-playback COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/jukebox_playback_test.py" "${CMAKE_BINARY_DIR}")
    add_test(NAME jukebox-scenario COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/jukebox_scenario_test.py" "${CMAKE_BINARY_DIR}")
    add_test(NAME jukebox-sdk COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/jukebox_sdk_test.py" "${CMAKE_BINARY_DIR}")

    add_executable(jukebox-midi-test tests/jukebox_midi_driver.c plugins/jukebox/midi.c)
    target_include_directories(jukebox-midi-test PRIVATE plugins/jukebox)
    target_compile_definitions(jukebox-midi-test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(jukebox-midi-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(jukebox-midi-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME jukebox-midi COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/jukebox_midi_test.py" "$<TARGET_FILE:jukebox-midi-test>")

    add_executable(sv-abi-snapshot tests/abi_snapshot.c)
    set_property(SOURCE tests/abi_snapshot.c APPEND PROPERTY OBJECT_DEPENDS "${SV_PLUGIN_HEADERS}")
    target_include_directories(sv-abi-snapshot PRIVATE src/plugins)
    target_compile_options(sv-abi-snapshot PRIVATE /W4 /WX /utf-8 /arch:IA32)
    set_target_properties(sv-abi-snapshot PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME abi-snapshot COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/test_abi_snapshot.py" "$<TARGET_FILE:sv-abi-snapshot>"
        "${CMAKE_SOURCE_DIR}/tests/abi_snapshot.txt")
    add_test(NAME sdk-runtime-download COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/sdk/test_runtime_download.py")
    add_test(NAME plugin-lifecycle COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/test_plugins.py" "${CMAKE_BINARY_DIR}")
    add_test(NAME plugin-inspect COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/sdk_inspect_test.py" "${CMAKE_BINARY_DIR}")
    add_test(NAME plugin-trust COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/sdk_trust_test.py" "${CMAKE_BINARY_DIR}")
    add_test(NAME plugin-approved-install COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/sdk_approval_install_test.py" "${CMAKE_BINARY_DIR}")
    add_test(NAME plugin-development COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/sdk_dev_plugins_test.py" "${CMAKE_BINARY_DIR}")
    add_test(NAME manifest-rules COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/test_manifest.py" "${CMAKE_BINARY_DIR}")
    add_test(NAME version-names COMMAND "${Python3_EXECUTABLE}" "-I"
        "${CMAKE_SOURCE_DIR}/tests/test_version_names.py")
    add_executable(sv-codec-test tests/codec_bounds.c tests/platform_support.c
        src/platform/x87_math.c src/platform/plat_imports.c ${SV_GAME_SOURCES})
    target_include_directories(sv-codec-test PRIVATE src/include src/platform src/common src/net src/game src/plugins)
    target_compile_definitions(sv-codec-test PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
    target_compile_options(sv-codec-test PRIVATE /W4 /WX /utf-8)
    target_link_options(sv-codec-test PRIVATE /LARGEADDRESSAWARE)
    set_target_properties(sv-codec-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME codec-bounds COMMAND sv-codec-test)
    add_executable(sv-game-test tests/plugin_game_test.c tests/plugin_cost.c tests/physics_bounds.c tests/platform_support.c
        src/platform/x87_math.c src/platform/plat_imports.c ${SV_GAME_SOURCES})
    target_include_directories(sv-game-test PRIVATE src/include src/platform src/common src/net src/game src/plugins)
    target_compile_definitions(sv-game-test PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
    target_compile_options(sv-game-test PRIVATE /W4 /WX /utf-8)
    target_link_options(sv-game-test PRIVATE /LARGEADDRESSAWARE)
    set_target_properties(sv-game-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_library(sv-game-read-fixture MODULE tests/plugin_game_fixture.c)
    target_include_directories(sv-game-read-fixture PRIVATE src/plugins)
    target_compile_definitions(sv-game-read-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-game-read-fixture PRIVATE /W4 /WX /utf-8)
    target_link_options(sv-game-read-fixture PRIVATE /EXPORT:sv_plugin_entry)
    set_target_properties(sv-game-read-fixture PROPERTIES PREFIX "" OUTPUT_NAME game-read-fixture
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_library(sv-game-runtime-fixture MODULE tests/plugin_runtime_fixture.c)
    target_include_directories(sv-game-runtime-fixture PRIVATE src/plugins)
    target_compile_definitions(sv-game-runtime-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-game-runtime-fixture PRIVATE /W4 /WX /utf-8)
    target_link_options(sv-game-runtime-fixture PRIVATE /EXPORT:sv_plugin_entry)
    set_target_properties(sv-game-runtime-fixture PROPERTIES PREFIX "" OUTPUT_NAME runtime-fixture
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME game-api COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/test_game_api.py" "${CMAKE_BINARY_DIR}")
    add_library(sv-cost-fixture MODULE tests/plugin_cost_fixture.c)
    target_include_directories(sv-cost-fixture PRIVATE src/plugins)
    target_compile_definitions(sv-cost-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-cost-fixture PRIVATE /W4 /WX /utf-8 /arch:IA32)
    target_link_options(sv-cost-fixture PRIVATE /EXPORT:sv_plugin_entry)
    set_target_properties(sv-cost-fixture PROPERTIES PREFIX "" OUTPUT_NAME cost-fixture
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME plugin-cost COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/test_plugin_cost.py" "${CMAKE_BINARY_DIR}")
    include(tests/systems/systems.cmake)
    add_executable(sv-handle-test tests/plugin_handles_test.c src/plugins/plugin_handles.c)
    target_include_directories(sv-handle-test PRIVATE src/plugins)
    target_compile_options(sv-handle-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(sv-handle-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME entity-handles COMMAND sv-handle-test)
    set(SV_LAUNCHER_TEST "${CMAKE_SOURCE_DIR}/build/launcher/launcher-update-tests.exe")
    add_custom_command(OUTPUT "${SV_LAUNCHER_TEST}"
        COMMAND cmd /c "${CMAKE_SOURCE_DIR}/tools/launcher/build.bat" test
        DEPENDS tools/launcher/Launcher.cs tools/launcher/LauncherUi.cs tools/launcher/LauncherUiMore.cs tools/launcher/LauncherServer.cs tools/launcher/LauncherSettings.cs tools/launcher/LauncherPlugins.cs tools/launcher/LauncherHelp.cs tools/launcher/LauncherStore.cs tools/launcher/help.md tools/launcher/index-public.pem tools/release/version-names.csv tools/launcher/build.bat tests/launcher_update_test.cs
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" VERBATIM)
    add_custom_target(sv-launcher-tests ALL DEPENDS "${SV_LAUNCHER_TEST}")
    add_test(NAME launcher-update-paths COMMAND "${SV_LAUNCHER_TEST}" "${CMAKE_BINARY_DIR}")
    add_test(NAME plugin-package-install COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tests/plugin_install_test.ps1" -BuildDir "${CMAKE_BINARY_DIR}")
    add_test(NAME sdk-package COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tests/sdk_package_test.ps1" -BuildDir "${CMAKE_BINARY_DIR}")
    add_test(NAME sdk-clean COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tests/sdk_clean_test.ps1" -BuildDir "${CMAKE_BINARY_DIR}")
    add_test(NAME package-contents COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tests/package_test.ps1" -BuildDir "${CMAKE_BINARY_DIR}")
endif()

if(BUILD_TESTING)
    target_sources(sv-game-test PRIVATE tests/plugin_world_test.c)
    target_sources(sv-game-read-fixture PRIVATE tests/plugin_world_fixture.c)
endif()
