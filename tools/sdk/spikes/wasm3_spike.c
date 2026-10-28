#include "spike.h"
#include "wasm3.h"

int main(int argc, char **argv)
{
    uint32_t length, result;
    unsigned char *data;
    IM3Environment environment = m3_NewEnvironment();
    IM3Runtime runtime = m3_NewRuntime(environment, 65536, NULL);
    IM3Module module;
    IM3Function call, grow, spin, fp;
    double fp_result;
    LARGE_INTEGER start, stop, frequency;
    unsigned short before, after;
    int i;
    REQUIRE(argc == 2 || argc == 3);
    REQUIRE(environment && runtime);
    data = read_module(argv[1], &length);
    REQUIRE(!m3_ParseModule(environment, &module, data, length));
    REQUIRE(!m3_LoadModule(runtime, module));
    REQUIRE(!m3_FindFunction(&call, runtime, "call"));
    REQUIRE(!m3_FindFunction(&grow, runtime, "grow"));
    REQUIRE(!m3_FindFunction(&spin, runtime, "spin"));
    REQUIRE(!m3_FindFunction(&fp, runtime, "fp"));
    set_control_word(0x027f);
    before = control_word();
    REQUIRE(QueryPerformanceFrequency(&frequency));
    QueryPerformanceCounter(&start);
    for (i = 0; i < 10000; ++i) {
        REQUIRE(!m3_CallV(call) && !m3_GetResultsV(call, &result) && result == 42);
    }
    QueryPerformanceCounter(&stop);
    after = control_word();
    printf("wasm3 call_us=%.3f cw_before=%04x cw_after=%04x\n",
        elapsed_us(start, stop, frequency, 10000), before, after);
    REQUIRE(before == after);
    set_control_word(0x067f);
    REQUIRE(!m3_CallV(fp, 82.0) && !m3_GetResultsV(fp, &fp_result) && fp_result == 42.0);
    REQUIRE(control_word() == 0x067f);
    printf("wasm3 fp_result=42 fp_cw=%04x\n", control_word());
    set_control_word(0x027f);
    REQUIRE(!m3_CallV(grow, 1) && !m3_GetResultsV(grow, &result) && result == 1);
    REQUIRE(!m3_CallV(grow, 1) && !m3_GetResultsV(grow, &result) && result == UINT32_MAX);
    REQUIRE(control_word() == before);
    printf("wasm3 memory_limit=131072\n");
    fflush(stdout);
    if (argc == 3)
        (void)m3_CallV(spin);
    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(environment);
    free(data);
    return 0;
}
