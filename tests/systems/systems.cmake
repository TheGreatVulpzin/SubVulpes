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
add_test(NAME systems-api COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tests/test_systems.py" "${CMAKE_BINARY_DIR}")
