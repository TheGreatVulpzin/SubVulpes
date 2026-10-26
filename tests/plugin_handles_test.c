#include "plugin_handles.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void check_zero(const SvHandlePool *pool)
{
    CHECK(!pool->index_to_slot);
    CHECK(!pool->slot_to_index);
    CHECK(!pool->generation);
    CHECK(!pool->free_next);
    CHECK(!pool->capacity);
    CHECK(!pool->free_head);
    CHECK(!pool->kind);
}

static void test_invalid(void)
{
    SvHandlePool pool = {0};
    CHECK(!sv_handle_pool_init(NULL, 1, 1));
    sv_handle_pool_destroy(NULL);
    sv_handle_pool_clear(NULL);
    sv_handle_pool_erase(NULL, 0);
    CHECK(!sv_handle_pool_bind(NULL, 0));
    CHECK(!sv_handle_pool_at(NULL, 0));
    CHECK(!sv_handle_pool_move(NULL, 0, 1));
    CHECK(sv_handle_pool_resolve(NULL, 1) == -1);
    sv_handle_pool_clear(&pool);
    sv_handle_pool_erase(&pool, 0);
    CHECK(!sv_handle_pool_bind(&pool, 0));
    CHECK(!sv_handle_pool_at(&pool, 0));
    CHECK(!sv_handle_pool_move(&pool, 0, 1));
    CHECK(sv_handle_pool_resolve(&pool, 1) == -1);
    CHECK(!sv_handle_pool_init(&pool, 0, 1));
    check_zero(&pool);
    CHECK(!sv_handle_pool_init(&pool, 1, 0));
    check_zero(&pool);
    CHECK(!sv_handle_pool_init(&pool, 1, UINT32_C(0x01000000)));
    check_zero(&pool);
    CHECK(!sv_handle_pool_init(&pool, 1, UINT32_MAX));
    check_zero(&pool);
    sv_handle_pool_destroy(&pool);
    sv_handle_pool_destroy(&pool);
    check_zero(&pool);
}

static void test_bind_and_reuse(void)
{
    SvHandlePool pool = {0};
    uint64_t first, second, replacement;
    CHECK(sv_handle_pool_init(&pool, 7, 2));
    CHECK(!sv_handle_pool_at(&pool, 0));
    CHECK(!sv_handle_pool_at(&pool, 2));
    CHECK(!sv_handle_pool_at(&pool, UINT32_MAX));
    CHECK(!sv_handle_pool_bind(&pool, 2));
    CHECK(!sv_handle_pool_bind(&pool, UINT32_MAX));
    first = sv_handle_pool_bind(&pool, 0);
    second = sv_handle_pool_bind(&pool, 1);
    CHECK(first && second && first != second);
    CHECK(sv_handle_pool_bind(&pool, 0) == first);
    CHECK(sv_handle_pool_bind(&pool, 1) == second);
    CHECK((uint8_t)(first >> 56) == 7);
    CHECK(sv_handle_pool_at(&pool, 0) == first);
    CHECK(sv_handle_pool_resolve(&pool, first) == 0);
    CHECK(sv_handle_pool_resolve(&pool, second) == 1);
    CHECK(sv_handle_pool_resolve(&pool, 0) == -1);
    CHECK(sv_handle_pool_resolve(&pool, UINT64_MAX) == -1);
    CHECK(sv_handle_pool_resolve(&pool, first ^ (UINT64_C(1) << 56)) == -1);
    CHECK(sv_handle_pool_resolve(&pool, first ^ (UINT64_C(1) << 24)) == -1);
    CHECK(sv_handle_pool_resolve(&pool, first & ~UINT64_C(0x00ffffff)) == -1);
    CHECK(sv_handle_pool_resolve(&pool,
        (first & ~UINT64_C(0x00ffffff)) | UINT64_C(3)) == -1);
    sv_handle_pool_erase(&pool, 2);
    sv_handle_pool_erase(&pool, UINT32_MAX);
    CHECK(sv_handle_pool_at(&pool, 0) == first);
    sv_handle_pool_erase(&pool, 0);
    sv_handle_pool_erase(&pool, 0);
    CHECK(!sv_handle_pool_at(&pool, 0));
    CHECK(sv_handle_pool_resolve(&pool, first) == -1);
    replacement = sv_handle_pool_bind(&pool, 0);
    CHECK(replacement && replacement != first);
    CHECK((replacement & UINT64_C(0x00ffffff)) ==
        (first & UINT64_C(0x00ffffff)));
    CHECK(sv_handle_pool_resolve(&pool, replacement) == 0);
    CHECK(sv_handle_pool_resolve(&pool, first) == -1);
    CHECK(sv_handle_pool_resolve(&pool, second) == 1);
    sv_handle_pool_clear(&pool);
    sv_handle_pool_clear(&pool);
    CHECK(sv_handle_pool_resolve(&pool, replacement) == -1);
    CHECK(sv_handle_pool_resolve(&pool, second) == -1);
    CHECK(!sv_handle_pool_at(&pool, 0));
    CHECK(!sv_handle_pool_at(&pool, 1));
    CHECK(sv_handle_pool_bind(&pool, 1));
    CHECK(sv_handle_pool_bind(&pool, 0));
    CHECK(sv_handle_pool_resolve(&pool, first) == -1);
    CHECK(sv_handle_pool_resolve(&pool, replacement) == -1);
    CHECK(sv_handle_pool_resolve(&pool, second) == -1);
    sv_handle_pool_destroy(&pool);
    check_zero(&pool);
}

static void test_move(void)
{
    SvHandlePool pool = {0};
    uint64_t first, removed, moved, added;
    CHECK(sv_handle_pool_init(&pool, 255, 4));
    first = sv_handle_pool_bind(&pool, 0);
    removed = sv_handle_pool_bind(&pool, 1);
    moved = sv_handle_pool_bind(&pool, 2);
    CHECK(!sv_handle_pool_move(&pool, 0, 0));
    CHECK(!sv_handle_pool_move(&pool, 0, 1));
    CHECK(!sv_handle_pool_move(&pool, 3, 0));
    CHECK(!sv_handle_pool_move(&pool, 4, 3));
    CHECK(!sv_handle_pool_move(&pool, 0, 4));
    CHECK(!sv_handle_pool_move(&pool, UINT32_MAX, 3));
    CHECK(!sv_handle_pool_move(&pool, 0, UINT32_MAX));
    CHECK(sv_handle_pool_at(&pool, 0) == first);
    CHECK(sv_handle_pool_at(&pool, 1) == removed);
    CHECK(sv_handle_pool_at(&pool, 2) == moved);
    sv_handle_pool_erase(&pool, 1);
    CHECK(sv_handle_pool_move(&pool, 2, 1));
    CHECK(sv_handle_pool_resolve(&pool, removed) == -1);
    CHECK(sv_handle_pool_resolve(&pool, moved) == 1);
    CHECK(sv_handle_pool_at(&pool, 1) == moved);
    CHECK(!sv_handle_pool_at(&pool, 2));
    added = sv_handle_pool_bind(&pool, 2);
    CHECK(added && added != removed && added != moved);
    CHECK(sv_handle_pool_resolve(&pool, added) == 2);
    CHECK(sv_handle_pool_resolve(&pool, moved) == 1);
    CHECK(sv_handle_pool_resolve(&pool, first) == 0);
    CHECK(sv_handle_pool_resolve(&pool, removed) == -1);
    sv_handle_pool_destroy(&pool);
}

static void test_retirement(void)
{
    SvHandlePool pool = {0};
    uint32_t slot;
    uint64_t before, final, survivor;
    CHECK(sv_handle_pool_init(&pool, 3, 2));
    CHECK(sv_handle_pool_bind(&pool, 0));
    slot = pool.index_to_slot[0];
    pool.generation[slot] = UINT32_MAX - 1;
    before = sv_handle_pool_at(&pool, 0);
    CHECK(sv_handle_pool_resolve(&pool, before) == 0);
    sv_handle_pool_erase(&pool, 0);
    CHECK(pool.generation[slot] == UINT32_MAX);
    CHECK(sv_handle_pool_resolve(&pool, before) == -1);
    sv_handle_pool_clear(&pool);
    final = sv_handle_pool_bind(&pool, 1);
    CHECK(final && pool.index_to_slot[1] == slot);
    CHECK((uint32_t)(final >> 24) == UINT32_MAX);
    CHECK(sv_handle_pool_resolve(&pool, final) == 1);
    sv_handle_pool_erase(&pool, 1);
    CHECK(pool.generation[slot] == UINT32_MAX);
    CHECK(sv_handle_pool_resolve(&pool, final) == -1);
    survivor = sv_handle_pool_bind(&pool, 0);
    CHECK(survivor && pool.index_to_slot[0] != slot);
    CHECK(!sv_handle_pool_bind(&pool, 1));
    CHECK(sv_handle_pool_resolve(&pool, survivor) == 0);
    sv_handle_pool_clear(&pool);
    CHECK(sv_handle_pool_bind(&pool, 1));
    CHECK(pool.index_to_slot[1] != slot);
    CHECK(!sv_handle_pool_bind(&pool, 0));
    CHECK(sv_handle_pool_resolve(&pool, before) == -1);
    CHECK(sv_handle_pool_resolve(&pool, final) == -1);
    CHECK(sv_handle_pool_resolve(&pool, survivor) == -1);
    sv_handle_pool_destroy(&pool);

    CHECK(sv_handle_pool_init(&pool, 3, 1));
    CHECK(sv_handle_pool_bind(&pool, 0));
    pool.generation[pool.index_to_slot[0]] = UINT32_MAX;
    final = sv_handle_pool_at(&pool, 0);
    sv_handle_pool_clear(&pool);
    sv_handle_pool_clear(&pool);
    CHECK(pool.generation[0] == UINT32_MAX);
    CHECK(sv_handle_pool_resolve(&pool, final) == -1);
    CHECK(!sv_handle_pool_bind(&pool, 0));
    sv_handle_pool_destroy(&pool);
}

#define PACKED_CAPACITY 32
#define PACKED_STEPS 2048
#define PACKED_HISTORY (PACKED_CAPACITY + PACKED_STEPS)

static uint32_t test_random(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static void test_packed_properties(void)
{
    SvHandlePool pool = {0};
    uint64_t handles[PACKED_HISTORY];
    uint32_t identities[PACKED_CAPACITY];
    int32_t expected[PACKED_HISTORY];
    uint32_t count = 0, made = 0, step, i;
    uint32_t random_state = UINT32_C(0x71c5a39d);
    CHECK(sv_handle_pool_init(&pool, 6, PACKED_CAPACITY));
    for (i = 0; i < PACKED_CAPACITY; ++i) {
        identities[count] = made;
        expected[made] = (int32_t)count;
        handles[made++] = sv_handle_pool_bind(&pool, count++);
    }
    for (step = 0; step < PACKED_STEPS; ++step) {
        uint32_t choice = test_random(&random_state);
        if (step % 97 == 96) {
            for (i = 0; i < count; ++i)
                expected[identities[i]] = -1;
            sv_handle_pool_clear(&pool);
            count = 0;
        } else if (!count || (count < PACKED_CAPACITY && (choice & 1))) {
            CHECK(made < PACKED_HISTORY);
            identities[count] = made;
            expected[made] = (int32_t)count;
            handles[made] = sv_handle_pool_bind(&pool, count);
            CHECK(handles[made]);
            ++made;
            ++count;
        } else {
            uint32_t removed = test_random(&random_state) % count;
            expected[identities[removed]] = -1;
            sv_handle_pool_erase(&pool, removed);
            --count;
            if (removed != count) {
                CHECK(sv_handle_pool_move(&pool, count, removed));
                identities[removed] = identities[count];
                expected[identities[removed]] = (int32_t)removed;
            }
        }
        for (i = 0; i < made; ++i) {
            int32_t index = sv_handle_pool_resolve(&pool, handles[i]);
            CHECK(index == expected[i]);
            if (index >= 0) {
                CHECK((uint32_t)index < count);
                CHECK(identities[(uint32_t)index] == i);
                CHECK(sv_handle_pool_at(&pool, (uint32_t)index) == handles[i]);
            }
        }
        for (i = count; i < PACKED_CAPACITY; ++i)
            CHECK(!sv_handle_pool_at(&pool, i));
    }
    sv_handle_pool_destroy(&pool);
}

int main(void)
{
    test_invalid();
    test_bind_and_reuse();
    test_move();
    test_retirement();
    test_packed_properties();
    puts("plugin handle tests passed");
    return EXIT_SUCCESS;
}
