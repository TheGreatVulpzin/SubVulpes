#include "spike.h"
#include "wasm_export.h"

typedef struct { HANDLE done; wasm_module_inst_t instance; } Watchdog;

static DWORD WINAPI stop_loop(void *data)
{
    Watchdog *watchdog = data;
    if (WaitForSingleObject(watchdog->done, 10) == WAIT_TIMEOUT)
        wasm_runtime_terminate(watchdog->instance);
    return 0;
}

int main(int argc, char **argv)
{
    static char pool[2097152];
    RuntimeInitArgs init = {0};
    struct InstantiationArgs2 *options;
    wasm_module_t module;
    wasm_module_inst_t instance;
    wasm_exec_env_t execution;
    wasm_function_inst_t call, grow, spin, fp;
    double fp_result = 82.0;
    char error[256] = {0};
    uint32_t length, arguments[2];
    unsigned char *data;
    LARGE_INTEGER start, stop, frequency;
    unsigned short before, after;
    int i;
    Watchdog watchdog;
    HANDLE thread;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, NULL, _IONBF, 0);
    REQUIRE(argc == 2);
    init.mem_alloc_type = Alloc_With_Pool;
    init.mem_alloc_option.pool.heap_buf = pool;
    init.mem_alloc_option.pool.heap_size = sizeof pool;
    REQUIRE(wasm_runtime_full_init(&init));
    data = read_module(argv[1], &length);
    module = wasm_runtime_load(data, length, error, sizeof error);
    if (!module)
        fprintf(stderr, "%s\n", error);
    REQUIRE(module != NULL);
    REQUIRE(wasm_runtime_instantiation_args_create(&options));
    wasm_runtime_instantiation_args_set_default_stack_size(options, 65536);
    wasm_runtime_instantiation_args_set_host_managed_heap_size(options, 0);
    wasm_runtime_instantiation_args_set_max_memory_pages(options, 2);
    instance = wasm_runtime_instantiate_ex2(module, options, error, sizeof error);
    if (!instance)
        fprintf(stderr, "%s\n", error);
    REQUIRE(instance != NULL);
    wasm_runtime_instantiation_args_destroy(options);
    execution = wasm_runtime_create_exec_env(instance, 65536);
    REQUIRE(execution != NULL);
    call = wasm_runtime_lookup_function(instance, "call");
    grow = wasm_runtime_lookup_function(instance, "grow");
    spin = wasm_runtime_lookup_function(instance, "spin");
    fp = wasm_runtime_lookup_function(instance, "fp");
    REQUIRE(call && grow && spin && fp);
    set_control_word(0x027f);
    before = control_word();
    REQUIRE(QueryPerformanceFrequency(&frequency));
    QueryPerformanceCounter(&start);
    for (i = 0; i < 10000; ++i) {
        arguments[0] = 0;
        REQUIRE(wasm_runtime_call_wasm(execution, call, 0, arguments) && arguments[0] == 42);
    }
    QueryPerformanceCounter(&stop);
    after = control_word();
    printf("wamr call_us=%.3f cw_before=%04x cw_after=%04x\n",
        elapsed_us(start, stop, frequency, 10000), before, after);
    REQUIRE(before == after);
    set_control_word(0x067f);
    memcpy(arguments, &fp_result, sizeof fp_result);
    REQUIRE(wasm_runtime_call_wasm(execution, fp, 2, arguments));
    memcpy(&fp_result, arguments, sizeof fp_result);
    REQUIRE(fp_result == 42.0 && control_word() == 0x067f);
    printf("wamr fp_result=42 fp_cw=%04x\n", control_word());
    set_control_word(0x027f);
    arguments[0] = 1;
    REQUIRE(wasm_runtime_call_wasm(execution, grow, 1, arguments) && arguments[0] == 1);
    arguments[0] = 1;
    REQUIRE(wasm_runtime_call_wasm(execution, grow, 1, arguments) && arguments[0] == UINT32_MAX);
    printf("wamr memory_limit=131072 runtime_pool=%zu\n", sizeof pool);
    watchdog.done = CreateEventA(NULL, TRUE, FALSE, NULL);
    watchdog.instance = instance;
    REQUIRE(watchdog.done != NULL);
    QueryPerformanceCounter(&start);
    thread = CreateThread(NULL, 0, stop_loop, &watchdog, 0, NULL);
    REQUIRE(thread != NULL);
    REQUIRE(!wasm_runtime_call_wasm(execution, spin, 0, arguments));
    QueryPerformanceCounter(&stop);
    REQUIRE(SetEvent(watchdog.done));
    REQUIRE(WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
    REQUIRE(CloseHandle(thread) && CloseHandle(watchdog.done));
    REQUIRE(strstr(wasm_runtime_get_exception(instance), "terminated by user"));
    REQUIRE(control_word() == before);
    printf("wamr loop_stop_us=%.3f reason=%s\n", elapsed_us(start, stop, frequency, 1), wasm_runtime_get_exception(instance));
    wasm_runtime_destroy_exec_env(execution);
    wasm_runtime_deinstantiate(instance);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();
    free(data);
    return 0;
}
