cmake_minimum_required(VERSION 3.20)
project(SubVulpes LANGUAGES C)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "SubVulpes needs MSVC x86 (32-bit). Use tools/build_hook.bat.")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

file(GLOB SV_GAME_SOURCES CONFIGURE_DEPENDS src/common/*.c src/net/*.c src/game/*.c src/server/*.c)
list(APPEND SV_GAME_SOURCES src/platform/os_calls.c)
add_executable(subvulpes-server
    src/standalone/main.c src/platform/x87_math.c
    src/platform/plat_imports.c ${SV_GAME_SOURCES})
target_include_directories(subvulpes-server PRIVATE
    src/include src/platform src/common src/net src/game)
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
target_link_options(subvulpes-server PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
set_target_properties(subvulpes-server PROPERTIES OUTPUT_NAME subvulpes_server)

set(SV_LAUNCHER "${CMAKE_SOURCE_DIR}/build/launcher/subvulpes_launcher.exe")
add_custom_command(OUTPUT "${SV_LAUNCHER}"
    COMMAND cmd /c "${CMAKE_SOURCE_DIR}/tools/launcher/build.bat"
    DEPENDS tools/launcher/Launcher.cs tools/launcher/build.bat tools/release/default-config.ini
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" VERBATIM)
add_custom_target(subvulpes-launcher ALL DEPENDS "${SV_LAUNCHER}")
