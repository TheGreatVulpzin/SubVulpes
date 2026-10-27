/* Validate the whole finance operation before emitting existing game events. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_economy.h"
#include "plugin_game_internal.h"
#include "logic.h"
#include "events.h"

static SvResult context(SvOwner owner)
{
    return sv_plugins_context(owner) && sv_game_is_ready() ? SV_OK : SV_STATE;
}

static int finite_value(float value)
{
    return value == value && value - value == 0.0f;
}

/* The stock event packs two positive prices in one signed integer. */
static int market_valid(int ignored)
{
    int j;
    for (j = 0; j < 5; ++j)
        if (j != ignored && (!finite_value(g_market.stocks[j].price) ||
            g_market.stocks[j].price < 10 || g_market.stocks[j].price > 32767))
            return 0;
    return 1;
}

static int holdings(uint32_t company, int64_t *total)
{
    int j;
    *total = 0;
    for (j = 0; j < SR_MAX_PLAYER; ++j)
        if (sr_players[j].active && (uint32_t)sr_players[j].team == company) {
            if (sr_players[j].stocks < 0)
                return 0;
            *total += sr_players[j].stocks;
        }
    return 1;
}

static SvResult SV_CALL read_company(SvOwner owner, uint32_t company, SvCompany *out)
{
    SvCompany copy = {sizeof copy};
    Stock *s;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (company >= SV_COMPANY_COUNT || !out || out->size < sizeof *out)
        return SV_INVALID;
    s = &g_market.stocks[company];
    copy.shares = s->shares;
    copy.price_int = s->price_int;
    copy.stake = s->stake;
    copy.price = s->price;
    copy.open_price = s->open_price;
    *out = copy;
    return SV_OK;
}

static SvResult SV_CALL set_price(SvOwner owner, uint32_t company, float price)
{
    int64_t total, factor = 1, shares;
    int j;
    Stock *s;
    float normalized = price;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (company >= SV_COMPANY_COUNT || !finite_value(price) || price < 10)
        return SV_INVALID;
    s = &g_market.stocks[company];
    if (!market_valid((int)company) || !holdings(company, &total) || s->shares < 1 || total > s->shares)
        return SV_STATE;
    shares = s->shares;
    while (normalized >= 500) {
        if (shares > INT32_MAX / 5)
            return SV_INVALID;
        shares *= 5;
        factor *= 5;
        /* Keep the game's split coefficient and float store. */
        normalized = (float)((double)normalized * 0.20000000298023224);
    }
    /* Each holding is nonnegative and no larger than the validated total share count. */
    s->shares = (int32_t)shares;
    s->price = normalized;
    for (j = 0; j < SR_MAX_PLAYER; ++j)
        if (sr_players[j].active && (uint32_t)sr_players[j].team == company) {
            sr_players[j].stocks = (int32_t)((int64_t)sr_players[j].stocks * factor);
            createevent_updateplayer_finance(j);
        }
    createevent_updatestocks();
    return SV_OK;
}

static SvResult SV_CALL set_shares(SvOwner owner, uint32_t company, int32_t shares)
{
    int64_t total;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (company >= SV_COMPANY_COUNT || shares < 1)
        return SV_INVALID;
    if (!market_valid(-1) || !holdings(company, &total))
        return SV_STATE;
    if (total > shares)
        return SV_INVALID;
    g_market.stocks[company].shares = shares;
    createevent_updatestocks();
    return SV_OK;
}

static SvResult SV_CALL add_stocks(SvOwner owner, SvEntity player, int32_t amount)
{
    int32_t index;
    uint32_t company;
    int64_t total, shares, held;
    Stock *s;
    if (context(owner) != SV_OK)
        return SV_STATE;
    index = sv_entity_resolve(SV_ENTITY_PLAYER, player);
    if (index < 0)
        return SV_NOT_FOUND;
    company = (uint32_t)sr_players[index].team;
    if (company >= SV_COMPANY_COUNT)
        return SV_STATE;
    s = &g_market.stocks[company];
    if (!market_valid(-1) || !holdings(company, &total) || s->shares < 1 || total > s->shares)
        return SV_STATE;
    shares = (int64_t)s->shares + amount;
    held = (int64_t)sr_players[index].stocks + amount;
    if (shares < 1 || shares > INT32_MAX || held < 0 || held > INT32_MAX)
        return SV_INVALID;
    s->shares = (int32_t)shares;
    sr_players[index].stocks = (int32_t)held;
    createevent_updateplayer_finance(index);
    createevent_updatestocks();
    return SV_OK;
}

const SvGameEconomy sv_game_economy = {
    sizeof(SvGameEconomy), SV_GAME_ECONOMY_VERSION,
    read_company, set_price, set_shares, add_stocks
};
