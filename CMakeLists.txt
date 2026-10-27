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
    tests/plugin_fixture.c tests/plugin_host_test.c tests/plugin_game_fixture.c tests/plugin_game_test.c
    APPEND PROPERTY OBJECT_DEPENDS "${SV_PLUGIN_HEADERS}")
set_property(SOURCE src/game/human_body.c src/game/human_arms.c src/game/human_pose.c
    src/game/humans.c src/game/item_physics.c src/game/objects.c src/game/wheels.c tests/physics_bounds.c
    APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/game/bonds.h;${CMAKE_SOURCE_DIR}/src/game/objects.h")
target_link_options(subvulpes-server PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
set_target_properties(subvulpes-server PROPERTIES OUTPUT_NAME subvulpes_server)

add_library(sv-status MODULE plugins/status/status.c)
target_include_directories(sv-status PRIVATE src/plugins)
target_compile_definitions(sv-status PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sv-status PRIVATE /W4 /WX /utf-8)
target_link_options(sv-status PRIVATE /EXPORT:sv_plugin_entry)
set_target_properties(sv-status PROPERTIES OUTPUT_NAME status PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/sv.status"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/sv.status")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/sv.status")
configure_file(plugins/status/plugin.ini "${CMAKE_BINARY_DIR}/plugins/sv.status/plugin.ini" COPYONLY)
configure_file(plugins/plugins.ini "${CMAKE_BINARY_DIR}/plugins/plugins.ini" COPYONLY)
add_custom_target(sv-official-approvals ALL
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tools/plugins/write_approval.ps1"
        -Binary "$<TARGET_FILE:sv-status>" -Id sv.status
        -Output "${CMAKE_BINARY_DIR}/plugins/native-approvals.ini"
    DEPENDS sv-status VERBATIM)

set(SV_LAUNCHER "${CMAKE_SOURCE_DIR}/build/launcher/subvulpes_launcher.exe")
add_custom_command(OUTPUT "${SV_LAUNCHER}"
    COMMAND cmd /c "${CMAKE_SOURCE_DIR}/tools/launcher/build.bat"
    DEPENDS tools/launcher/Launcher.cs tools/launcher/build.bat tools/release/default-config.ini
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
    add_executable(sv-plugin-test tests/plugin_host_test.c src/plugins/plugin_host.c
        src/plugins/plugin_files.c src/plugins/plugin_control.c src/plugins/plugin_events.c)
    target_include_directories(sv-plugin-test PRIVATE src/plugins)
    target_compile_definitions(sv-plugin-test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sv-plugin-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(sv-plugin-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME plugin-lifecycle COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/test_plugins.py" "${CMAKE_BINARY_DIR}")
    add_executable(sv-codec-test tests/codec_bounds.c tests/platform_support.c
        src/platform/x87_math.c src/platform/plat_imports.c ${SV_GAME_SOURCES})
    target_include_directories(sv-codec-test PRIVATE src/include src/platform src/common src/net src/game src/plugins)
    target_compile_definitions(sv-codec-test PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
    target_compile_options(sv-codec-test PRIVATE /W4 /WX /utf-8)
    target_link_options(sv-codec-test PRIVATE /LARGEADDRESSAWARE)
    set_target_properties(sv-codec-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME codec-bounds COMMAND sv-codec-test)
    add_executable(sv-game-test tests/plugin_game_test.c tests/physics_bounds.c tests/platform_support.c
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
    add_test(NAME game-api COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/test_game_api.py" "${CMAKE_BINARY_DIR}")
    include(tests/systems/systems.cmake)
    add_executable(sv-handle-test tests/plugin_handles_test.c src/plugins/plugin_handles.c)
    target_include_directories(sv-handle-test PRIVATE src/plugins)
    target_compile_options(sv-handle-test PRIVATE /W4 /WX /utf-8)
    set_target_properties(sv-handle-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME entity-handles COMMAND sv-handle-test)
    set(SV_LAUNCHER_TEST "${CMAKE_SOURCE_DIR}/build/launcher/launcher-update-tests.exe")
    add_custom_command(OUTPUT "${SV_LAUNCHER_TEST}"
        COMMAND cmd /c "${CMAKE_SOURCE_DIR}/tools/launcher/build.bat" test
        DEPENDS tools/launcher/Launcher.cs tools/launcher/build.bat tests/launcher_update_test.cs
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" VERBATIM)
    add_custom_target(sv-launcher-tests ALL DEPENDS "${SV_LAUNCHER_TEST}")
    add_test(NAME launcher-update-paths COMMAND "${SV_LAUNCHER_TEST}")
    add_test(NAME plugin-package-install COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tests/plugin_install_test.ps1" -BuildDir "${CMAKE_BINARY_DIR}")
    add_test(NAME package-contents COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tests/package_test.ps1")
endif()
