#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "packet.h"
#include "server.h"
#include "sr_math.h"

#define CHECK(test) do { \
    if (!(test)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); \
        exit(1); \
    } \
} while (0)

typedef void (*PackObjects)(int conn, int player);

static const uint32_t trailer = 0xa65d;

static uint32_t read_bits(uint32_t bits)
{
    uint32_t value = 0;
    CHECK(pbitread(&value, bits));
    return value;
}

static void reset_scene(void)
{
    memset(sr_humans, 0, sizeof(sr_data_human));
    memset(sr_objects, 0, sizeof(sr_data_object));
    memset(sr_items, 0, sizeof(sr_data_item));
    memset(sr_data_cityobject, 0, sizeof(sr_data_cityobject));
    memset(&sr_connections[0], 0, sizeof(sr_connections[0]));
    memset(&sr_objectpacket, 0, sizeof(sr_objectpacket));
    memset(&sr_bsdsocket, 0, sizeof(sr_bsdsocket));
    sr_numofevents = 0;
}

static void start_reading(PackObjects pack)
{
    pack(0, -1);
    CHECK(pbitwrite(&trailer, 16));
    sr_bsdsocket.recv_size = sr_bsdsocket.send_size + (sr_bsdsocket.send_bit != 0);
    CHECK(sr_bsdsocket.recv_size <= (int)sizeof(sr_bsdsocket.recv_buf));
    memcpy(sr_bsdsocket.recv_buf, sr_bsdsocket.send_buf, (size_t)sr_bsdsocket.recv_size);
}

static void check_trailer(void)
{
    CHECK(read_bits(16) == trailer);
    CHECK(sr_bsdsocket.recv_pos == sr_bsdsocket.send_size);
    CHECK(sr_bsdsocket.recv_bit == sr_bsdsocket.send_bit);
    CHECK(sr_connections[0].sequence == 1);
}

static void check_position(const vec3 *expected)
{
    vec3 value;
    CHECK(cs_pread(&value, sizeof(float), 3));
    CHECK(memcmp(&value, expected, sizeof(value)) == 0);
}

/* Read payload bits through the real reader, including byte alignment at positions. */
static void skip_orientation(void)
{
    (void)read_bits(31);
    (void)read_bits(31);
}

static void test_health(PackObjects pack)
{
    static const struct {
        int health;
        uint32_t wire;
    } cases[] = {
        { INT_MIN, 0 }, { -11, 0 }, { -10, 0 }, { -9, 1 },
        { -1, 9 }, { 0, 10 }, { 1, 11 }, { 100, 110 },
        { 116, 126 }, { 117, 127 }, { 118, 127 }, { INT_MAX, 127 }
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Human *hm = &sr_humans[0];
        int b;
        reset_scene();
        hm->active = 1;
        hm->health = cases[i].health;
        hm->vehicle_id = -1;
        hm->vehicle_seat = 3;
        hm->bleeding = 1;
        hm->bones[0].pos.x = 3.0f;
        hm->bones[0].pos.y = 4.0f;
        hm->bones[0].pos.z = 5.0f;
        hm->bones[0].rot[1].y = 1.0f;
        hm->bones[0].rot[2].z = 1.0f;
        start_reading(pack);
        CHECK(sr_objectpacket.num_humans == 1);
        CHECK(hm->health == cases[i].health);
        CHECK(read_bits(8) == 1);
        CHECK(read_bits(10) == 0);
        CHECK(read_bits(1) == 0);
        CHECK(read_bits(8) == 255);
        CHECK(read_bits(4) == 3);
        CHECK(read_bits(7) == cases[i].wire);
        CHECK(read_bits(1) == 1);
        check_position(&hm->bones[0].pos);
        skip_orientation();
        for (b = 1; b < 15; b++) {
            CHECK(read_bits(16) == 0);
            CHECK(read_bits(16) == 0);
            CHECK(read_bits(16) == 0);
        }
        CHECK(read_bits(8) == 0); /* objects */
        CHECK(read_bits(8) == 0); /* items */
        CHECK(read_bits(8) == 0); /* city objects */
        check_trailer();
    }
}

static void make_item(int id)
{
    Item *it = &sr_items[id];
    it->active = 1;
    it->parent_item = -1;
    it->parent_human = -1;
    it->vel.x = 1.0f;
    it->pos.x = (float)id / 1024.0f;
    it->rot[1].y = 1.0f;
    it->rot[2].z = 1.0f;
    if (id & 1) {
        it->type = 0x10;
        it->last_bill = 7;
        it->num_bills = 9;
        it->bills_mask = 0x25a5a5a5;
    }
}

static void test_items(PackObjects pack, int count, int exclude_first)
{
    const int first = exclude_first ? 4 : 0;
    const int expected = count - first > 255 ? 255 : count - first;
    CityObjectRec *city = &sr_data_cityobject[7];
    int i;
    reset_scene();
    for (i = 0; i < count; i++)
        make_item(i);
    if (exclude_first) {
        sr_items[0].active = 0;
        sr_items[1].parent_item = 0;
        sr_items[2].parent_human = 0;
        sr_items[2].parent_slot = 2;
        /* A still, distant item fails the normal distance and motion checks. */
        sr_items[3].pos.x = 512.0f;
        sr_items[3].vel.x = 0.0f;
    }
    city->active = 1;
    city->pos.x = 11.0f;
    city->pos.y = 12.0f;
    city->pos.z = 13.0f;
    city->rot[1].y = 1.0f;
    city->rot[2].z = 1.0f;
    city->num_floors = 83;
    city->floor = 71;
    city->net_scalar = 0.5f;
    /* Detect writes beyond the 255 selected entries, including the spare slot. */
    sr_objectpacket.items[255] = -12345;
    sr_objectpacket.num_candidates = 0;
    sr_objectpacket.candidates[0].kind = -23456;
    start_reading(pack);
    CHECK(sr_objectpacket.num_items == expected);
    CHECK(sr_objectpacket.items[255] == -12345);
    CHECK(sr_objectpacket.candidates[0].kind == -23456);
    CHECK(read_bits(8) == 0); /* humans */
    CHECK(read_bits(8) == 0); /* objects */
    CHECK(read_bits(8) == (uint32_t)expected);
    for (i = 0; i < expected; i++) {
        const int id = first + i;
        const Item *it = &sr_items[id];
        CHECK(sr_objectpacket.items[i] == id);
        CHECK(read_bits(10) == (uint32_t)id);
        check_position(&it->pos);
        skip_orientation();
        CHECK(read_bits(1) == (uint32_t)(id & 1));
        if (id & 1) {
            CHECK(read_bits(4) == 7);
            CHECK(read_bits(4) == 9);
            CHECK(read_bits(30) == 0x25a5a5a5);
        }
    }
    CHECK(read_bits(8) == 1); /* city objects follow the item list */
    CHECK(read_bits(10) == 7);
    check_position(&city->pos);
    skip_orientation();
    CHECK(read_bits(7) == 83);
    CHECK(read_bits(7) == 71);
    CHECK(read_bits(6) == 16);
    check_trailer();
}

int main(void)
{
    static const PackObjects packers[] = { packobjectpacket, packobjectpacket2 };
    static const int counts[] = { 0, 1, 254, 255, 256, 300, SR_MAX_ITEM };
    size_t p, n;
    x87_init();
    for (p = 0; p < sizeof(packers) / sizeof(packers[0]); p++) {
        test_health(packers[p]);
        for (n = 0; n < sizeof(counts) / sizeof(counts[0]); n++)
            test_items(packers[p], counts[n], 0);
        test_items(packers[p], 300, 1);
    }
    puts("codec bounds passed");
    return 0;
}
