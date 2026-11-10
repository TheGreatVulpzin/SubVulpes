# The test program and the plugin of the system areas. Included from CMakeLists.txt.
# Add a file named <area>_host.c or <area>_plugin.c in tests/systems and it joins the build.
file(GLOB SV_SYSTEMS_HOST CONFIGURE_DEPENDS tests/systems/*_host.c)
file(GLOB SV_SYSTEMS_PLUGIN CONFIGURE_DEPENDS tests/systems/*_plugin.c)
set_property(SOURCE tests/systems/main.c tests/systems/world.c ${SV_SYSTEMS_HOST} tests/systems/fixture.c ${SV_SYSTEMS_PLUGIN}
    APPEND PROPERTY OBJECT_DEPENDS "${SV_PLUGIN_HEADERS};${SV_STORAGE_HEADERS}")
add_executable(sv-systems-test tests/systems/main.c tests/systems/world.c ${SV_SYSTEMS_HOST}
    tests/platform_support.c src/platform/x87_math.c src/platform/plat_imports.c ${SV_GAME_SOURCES})
target_include_directories(sv-systems-test PRIVATE src/include src/platform src/common src/net src/game src/plugins tests/systems)
target_compile_definitions(sv-systems-test PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
target_compile_options(sv-systems-test PRIVATE /W4 /WX /utf-8)
target_link_options(sv-systems-test PRIVATE /LARGEADDRESSAWARE)
set_target_properties(sv-systems-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_library(sv-systems-fixture MODULE tests/systems/fixture.c ${SV_SYSTEMS_PLUGIN})
target_include_directories(sv-systems-fixture PRIVATE src/plugins tests/systems)
target_compile_definitions(sv-systems-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sv-systems-fixture PRIVATE /W4 /WX /utf-8)
target_link_options(sv-systems-fixture PRIVATE /EXPORT:sv_plugin_entry)
set_target_properties(sv-systems-fixture PROPERTIES PREFIX "" OUTPUT_NAME systems-fixture
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
# The plugin test runner (work package WP4): the game sources, a small world and a scenario interpreter.
add_executable(subvulpes-plugin-test tests/runner/runner.c
    tests/platform_support.c src/platform/x87_math.c src/platform/plat_imports.c ${SV_GAME_SOURCES})
target_include_directories(subvulpes-plugin-test PRIVATE src/include src/platform src/common src/net src/game src/plugins)
target_compile_definitions(subvulpes-plugin-test PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
target_compile_options(subvulpes-plugin-test PRIVATE /W4 /WX /utf-8)
target_link_options(subvulpes-plugin-test PRIVATE /LARGEADDRESSAWARE)
set_target_properties(subvulpes-plugin-test PROPERTIES OUTPUT_NAME subvulpes_plugin_test
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
sv_add_plugin(sv-probe ID sv.probe MANIFEST "${CMAKE_SOURCE_DIR}/tests/runner/probe/plugin.ini"
    SOURCES tests/runner/probe_plugin.c)
add_custom_target(sdk-package
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/tools/sdk/make_sdk.ps1"
        -Version "${SV_SDK_VERSION}" -OutputDir "${CMAKE_BINARY_DIR}/packages"
        -Runner "$<TARGET_FILE:subvulpes-plugin-test>" -ProbeDir "${CMAKE_BINARY_DIR}/plugins/sv.probe"
    DEPENDS subvulpes-plugin-test sv-probe VERBATIM)
add_test(NAME plugin-scenarios COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tests/scenario_test.py" "${CMAKE_BINARY_DIR}")
add_test(NAME systems-api COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tests/test_systems.py" "${CMAKE_BINARY_DIR}")

add_executable(sv-n-bots-test tests/n_bots/main.c tests/systems/world.c
    tests/platform_support.c src/platform/x87_math.c src/platform/plat_imports.c ${SV_GAME_SOURCES})
target_include_directories(sv-n-bots-test PRIVATE src/include src/platform src/common src/net src/game src/plugins tests/systems)
target_compile_definitions(sv-n-bots-test PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
target_compile_options(sv-n-bots-test PRIVATE /W4 /WX /utf-8)
target_link_options(sv-n-bots-test PRIVATE /LARGEADDRESSAWARE)
set_target_properties(sv-n-bots-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_library(sv-n-bots-fixture MODULE tests/n_bots/plugin.c)
target_include_directories(sv-n-bots-fixture PRIVATE src/plugins tests/systems)
target_compile_definitions(sv-n-bots-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sv-n-bots-fixture PRIVATE /W4 /WX /utf-8)
target_link_options(sv-n-bots-fixture PRIVATE /EXPORT:sv_plugin_entry)
set_target_properties(sv-n-bots-fixture PROPERTIES PREFIX "" OUTPUT_NAME n-bots-fixture
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME n-bots-api COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_n_bots.py" "${CMAKE_BINARY_DIR}")
add_executable(sv-n-roads-test tests/n_roads/main.c tests/systems/world.c
    tests/platform_support.c src/platform/x87_math.c src/platform/plat_imports.c ${SV_GAME_SOURCES})
target_include_directories(sv-n-roads-test PRIVATE src/include src/platform src/common src/net src/game src/plugins tests/systems)
target_compile_definitions(sv-n-roads-test PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
target_compile_options(sv-n-roads-test PRIVATE /W4 /WX /utf-8)
target_link_options(sv-n-roads-test PRIVATE /LARGEADDRESSAWARE)
set_target_properties(sv-n-roads-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_library(sv-n-roads-fixture MODULE tests/n_roads/plugin.c)
target_include_directories(sv-n-roads-fixture PRIVATE src/plugins tests/systems)
target_compile_definitions(sv-n-roads-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sv-n-roads-fixture PRIVATE /W4 /WX /utf-8)
target_link_options(sv-n-roads-fixture PRIVATE /EXPORT:sv_plugin_entry)
set_target_properties(sv-n-roads-fixture PROPERTIES PREFIX "" OUTPUT_NAME n-roads-fixture
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME n-roads-api COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_n_roads.py" "${CMAKE_BINARY_DIR}")
add_executable(sv-n-mission-test tests/n_mission/main.c tests/systems/world.c
    tests/platform_support.c src/platform/x87_math.c src/platform/plat_imports.c ${SV_GAME_SOURCES})
target_include_directories(sv-n-mission-test PRIVATE src/include src/platform src/common src/net src/game src/plugins tests/systems)
target_compile_definitions(sv-n-mission-test PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
target_compile_options(sv-n-mission-test PRIVATE /W4 /WX /utf-8)
target_link_options(sv-n-mission-test PRIVATE /LARGEADDRESSAWARE)
set_target_properties(sv-n-mission-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_library(sv-n-mission-fixture MODULE tests/n_mission/plugin.c)
target_include_directories(sv-n-mission-fixture PRIVATE src/plugins tests/systems)
target_compile_definitions(sv-n-mission-fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sv-n-mission-fixture PRIVATE /W4 /WX /utf-8)
target_link_options(sv-n-mission-fixture PRIVATE /EXPORT:sv_plugin_entry)
set_target_properties(sv-n-mission-fixture PROPERTIES PREFIX "" OUTPUT_NAME n-mission-fixture
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME n-mission-api COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_n_mission.py" "${CMAKE_BINARY_DIR}")
