cmake_minimum_required(VERSION 3.20)
project(my_plugin C)

# The folder tools/ sits next to this folder in the SDK. The function sv_add_plugin builds the DLL,
# and the target my-hello-package makes the .svplugin file. Pass -DSV_SERVER_EXE=PATH\subvulpes_server.exe for it.
include("${CMAKE_CURRENT_SOURCE_DIR}/../tools/sv_plugin.cmake")
sv_add_plugin(my-hello ID my.hello MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini" SOURCES hello.c)
