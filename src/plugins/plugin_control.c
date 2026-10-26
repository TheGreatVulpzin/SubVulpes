/* A bounded stdin queue keeps console I/O outside the simulation thread. */
#include "plugin_internal.h"
#include <string.h>

static CRITICAL_SECTION mutex;
static HANDLE reader;
static volatile LONG closing;
static char queue[16][301];
static unsigned head, count;

static DWORD WINAPI read_input(void *unused)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    char line[301], ch;
    DWORD bytes;
    size_t length = 0;
    int overflow = 0;
    (void)unused;
    while (!InterlockedCompareExchange(&closing, 0, 0) && ReadFile(input, &ch, 1, &bytes, NULL) && bytes) {
        if (ch == '\r')
            continue;
        if (ch == '\n') {
            line[length] = 0;
            EnterCriticalSection(&mutex);
            if (!overflow && length && count < 16) {
                strcpy(queue[(head + count) % 16], line);
                ++count;
            }
            LeaveCriticalSection(&mutex);
            length = 0;
            overflow = 0;
        } else if (length < sizeof line - 1) {
            line[length++] = ch;
        } else {
            overflow = 1;
        }
    }
    return 0;
}

void sv_control_start(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (reader || !input || input == INVALID_HANDLE_VALUE)
        return;
    InitializeCriticalSection(&mutex);
    InterlockedExchange(&closing, 0);
    head = count = 0;
    reader = CreateThread(NULL, 0, read_input, NULL, 0, NULL);
    if (!reader)
        DeleteCriticalSection(&mutex);
}

void sv_control_poll(void)
{
    unsigned i;
    if (!reader)
        return;
    for (i = 0; i < 8; ++i) {
        char line[301];
        EnterCriticalSection(&mutex);
        if (!count) {
            LeaveCriticalSection(&mutex);
            break;
        }
        strcpy(line, queue[head]);
        head = (head + 1) % 16;
        --count;
        LeaveCriticalSection(&mutex);
        if (!sv_plugins_command(0, line))
            sv_log("Unknown local command.");
    }
}

void sv_control_stop(void)
{
    if (!reader)
        return;
    InterlockedExchange(&closing, 1);
    /* Repeat cancellation until the reader passes its final blocking read. */
    while (WaitForSingleObject(reader, 0) == WAIT_TIMEOUT) {
        CancelSynchronousIo(reader);
        if (WaitForSingleObject(reader, 10) != WAIT_TIMEOUT)
            break;
    }
    CloseHandle(reader);
    reader = NULL;
    DeleteCriticalSection(&mutex);
}
