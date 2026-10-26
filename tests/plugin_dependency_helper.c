#define WIN32_LEAN_AND_MEAN
#include <windows.h>

__declspec(dllexport) int __cdecl sv_test_helper(void)
{
    return 42;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    char path[MAX_PATH];
    DWORD length;
    HANDLE file;
    (void)instance;
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;
    length = GetEnvironmentVariableA("SV_PLUGIN_TEST_MARKER", path, sizeof path);
    if (!length || length >= sizeof path)
        return TRUE;
    file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
    return TRUE;
}
