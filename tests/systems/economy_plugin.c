#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_economy.h"

typedef struct { const SvGameEconomy *api; SvOwner owner; SvEntity player; } Args;

static DWORD WINAPI wrong_thread(void *data)
{
    Args *a = data;
    SvCompany out = {sizeof out};
    STEP(a->api->company(a->owner, 0, &out) == SV_STATE);
    STEP(a->api->set_price(a->owner, 0, 100) == SV_STATE);
    STEP(a->api->set_shares(a->owner, 0, 100) == SV_STATE);
    STEP(a->api->add_stocks(a->owner, a->player, 1) == SV_STATE);
    return 0;
}

int economy_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameEconomy *api;
    const SvGameRead *read;
    SvCompany out = {sizeof out}, saved;
    SvEntity entities[256], player = 0, human = 0, malformed = 0, overflow = 0, wrong_team = 0;
    SvPlayerState p = {sizeof p};
    uint32_t count, next, j;
    Args args;
    HANDLE thread;
    DWORD code;
    uint32_t bad_bits = 0x7fc00000u;
    float bad_price;
    STEP(host->get_api(owner, SV_API_ECONOMY, SV_GAME_ECONOMY_VERSION, &table) == SV_OK);
    api = (const SvGameEconomy *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_ECONOMY_VERSION);
    STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_OK);
    read = (const SvGameRead *)table;
    STEP(read->entities(owner, SV_ENTITY_PLAYER, 0, entities, 256, &count, &next) == SV_OK);
    for (j = 0; j < count; ++j) {
        STEP(read->player(owner, entities[j], &p) == SV_OK);
        if (!strcmp(p.name, "Systems test")) { player = entities[j]; human = p.human; }
        if (!strcmp(p.name, "Econ malformed")) malformed = entities[j];
        if (!strcmp(p.name, "Econ overflow")) overflow = entities[j];
        if (!strcmp(p.name, "Econ wrong team")) wrong_team = entities[j];
    }
    STEP(player && human && malformed && overflow && wrong_team);
    args = (Args){api, owner, player};
    thread = CreateThread(NULL, 0, wrong_thread, &args, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    STEP(api->company(0, 0, &out) == SV_STATE);
    STEP(api->set_price(0, 0, 100) == SV_STATE);
    STEP(api->set_shares(0, 0, 100) == SV_STATE);
    STEP(api->add_stocks(0, player, 1) == SV_STATE);
    saved = out;
    STEP(api->company(owner, 5, &out) == SV_INVALID);
    STEP(!memcmp(&saved, &out, sizeof out));
    STEP(api->company(owner, 0, NULL) == SV_INVALID);
    out.size = 0;
    saved = out;
    STEP(api->company(owner, 0, &out) == SV_INVALID);
    STEP(!memcmp(&saved, &out, sizeof out));
    out.size = sizeof out;
    STEP(api->company(owner, 0, &out) == SV_OK);
    STEP(out.shares == 100 && out.price == 100 && out.stake == 10 && out.open_price == 100);
    STEP(api->set_price(owner, 5, 100) == SV_INVALID);
    STEP(api->set_shares(owner, 5, 100) == SV_INVALID);
    STEP(api->set_price(owner, 0, 9) == SV_INVALID);
    memcpy(&bad_price, &bad_bits, sizeof bad_price);
    STEP(api->set_price(owner, 0, bad_price) == SV_INVALID);
    bad_bits = 0x7f800000u;
    memcpy(&bad_price, &bad_bits, sizeof bad_price);
    STEP(api->set_price(owner, 0, bad_price) == SV_INVALID);
    STEP(api->set_shares(owner, 0, 0) == SV_INVALID);
    STEP(api->add_stocks(owner, human, 1) == SV_NOT_FOUND);
    STEP(api->add_stocks(owner, wrong_team, 1) == SV_STATE);
    STEP(api->set_price(owner, 0, 100) == SV_STATE);
    STEP(api->set_shares(owner, 0, 100) == SV_STATE);
    STEP(api->add_stocks(owner, player, 1) == SV_STATE);
    STEP(api->set_price(owner, 4, 100) == SV_OK);
    STEP(api->set_shares(owner, 4, 0) == SV_INVALID);
    STEP(api->add_stocks(owner, overflow, -INT32_MAX) == SV_INVALID);
    STEP(api->set_price(owner, 0, 3.402823466e38f) == SV_INVALID);
    STEP(api->add_stocks(owner, wrong_team, 1) == SV_STATE);
    STEP(api->set_price(owner, 2, 100) == SV_STATE);
    STEP(api->set_shares(owner, 2, 100) == SV_STATE);
    STEP(api->add_stocks(owner, malformed, 1) == SV_STATE);
    STEP(api->set_shares(owner, 0, 3) == SV_INVALID);
    STEP(api->set_price(owner, 1, 500) == SV_INVALID);
    STEP(api->add_stocks(owner, overflow, 1) == SV_INVALID);
    STEP(api->add_stocks(owner, player, -5) == SV_INVALID);
    STEP(api->company(owner, 0, &out) == SV_OK && out.shares == 100);
    STEP(read->player(owner, player, &p) == SV_OK && p.stocks == 4 && p.money == 12345);
    STEP(api->set_price(owner, 0, 499) == SV_OK);
    STEP(api->company(owner, 0, &out) == SV_OK && out.shares == 100 && out.price == 499);
    STEP(api->set_price(owner, 0, 500) == SV_OK);
    STEP(api->company(owner, 0, &out) == SV_OK && out.shares == 500 && out.price == 100);
    STEP(read->player(owner, player, &p) == SV_OK && p.stocks == 20);
    STEP(api->add_stocks(owner, player, 3) == SV_OK);
    STEP(api->set_shares(owner, 0, 1000) == SV_OK);
    STEP(api->add_stocks(owner, player, -2) == SV_OK);
    STEP(api->add_stocks(owner, player, 0) == SV_OK);
    STEP(api->set_price(owner, 4, 2500) == SV_OK);
    return 0;
}
