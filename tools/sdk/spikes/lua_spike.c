#include "spike.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

typedef struct { size_t used, peak, limit; } Memory;

static void *allocate(void *user, void *pointer, size_t old, size_t size)
{
    Memory *memory = user;
    void *next;
    if (!pointer)
        old = 0;
    if (!size) {
        free(pointer);
        memory->used -= old;
        return NULL;
    }
    if (size > memory->limit - (memory->used - old))
        return NULL;
    next = realloc(pointer, size);
    if (next) {
        memory->used = memory->used - old + size;
        if (memory->used > memory->peak)
            memory->peak = memory->used;
    }
    return next;
}

static void stop_loop(lua_State *state, lua_Debug *debug)
{
    (void)debug;
    luaL_error(state, "instruction limit");
}

static int run_text(lua_State *state, const char *text)
{
    int result = luaL_loadbufferx(state, text, strlen(text), "spike", "t");
    return result != LUA_OK ? result : lua_pcall(state, 0, LUA_MULTRET, 0);
}

int main(void)
{
    Memory memory = {0, 0, 131072};
    lua_State *state = lua_newstate(allocate, &memory);
    LARGE_INTEGER start, stop, frequency;
    unsigned short before, after;
    int i, result;
    REQUIRE(state != NULL);
    luaL_openlibs(state);
    set_control_word(0x027f);
    before = control_word();
    REQUIRE(run_text(state, "function call() return 42 end function fp(x) return x * 0.5 + 1 end") == LUA_OK);
    REQUIRE(QueryPerformanceFrequency(&frequency));
    QueryPerformanceCounter(&start);
    for (i = 0; i < 10000; ++i) {
        lua_getglobal(state, "call");
        REQUIRE(lua_pcall(state, 0, 1, 0) == LUA_OK && lua_tointeger(state, -1) == 42);
        lua_pop(state, 1);
    }
    QueryPerformanceCounter(&stop);
    after = control_word();
    printf("lua call_us=%.3f cw_before=%04x cw_after=%04x\n",
        elapsed_us(start, stop, frequency, 10000), before, after);
    REQUIRE(before == after);
    set_control_word(0x067f);
    lua_getglobal(state, "fp");
    lua_pushnumber(state, 82.0);
    REQUIRE(lua_pcall(state, 1, 1, 0) == LUA_OK && lua_tonumber(state, -1) == 42.0);
    REQUIRE(control_word() == 0x067f);
    lua_pop(state, 1);
    printf("lua fp_result=42 fp_cw=%04x\n", control_word());
    set_control_word(0x027f);
    result = run_text(state, "return string.rep('x', 1048576)");
    printf("lua allocation_result=%d reason=%s\n", result, result == LUA_OK ? "allocation allowed" : lua_tostring(state, -1));
    REQUIRE((result == LUA_ERRMEM || result == LUA_ERRRUN) &&
        strstr(lua_tostring(state, -1), "memory") && memory.peak <= memory.limit);
    lua_pop(state, 1);
    REQUIRE(control_word() == before);
    printf("lua memory_limit=%zu peak=%zu\n", memory.limit, memory.peak);
    lua_sethook(state, stop_loop, LUA_MASKCOUNT, 10000);
    QueryPerformanceCounter(&start);
    result = run_text(state, "while true do end");
    QueryPerformanceCounter(&stop);
    REQUIRE(result == LUA_ERRRUN && strstr(lua_tostring(state, -1), "instruction limit"));
    REQUIRE(control_word() == before);
    printf("lua loop_stop_us=%.3f reason=%s\n", elapsed_us(start, stop, frequency, 1), lua_tostring(state, -1));
    lua_close(state);
    REQUIRE(memory.used == 0);
    return 0;
}
