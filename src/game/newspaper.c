/* News text and the account money ranking. */
#include "buildings.h"

#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "sr_server.h"
#include "util.h"

#define g_newspaper sr_data_newspaper
#define g_newspaper_line sr_data_newspaper_line
#define g_newspaper_money sr_data_newspaper_money
#define g_parser sr_data_parser
#define g_radix sr_data_radix
#define g_numofaccounts (*((int *)&sr_data_accounts.count))
#define g_accounts ((Account *)&sr_data_accounts.records)

/* The server sends one news line as two events. The first part starts at byte 0 and the second at byte 0x3f. */
void newspaper_send_line(int line)
{
    const char *text = (const char *)g_newspaper.lines + line * 256;
    sr_createevent_message(3, text, line, 0);
    sr_createevent_message(3, text + 0x3f, line, 1);
}

/* Replaces a news line, or adds one at the end, and sends it to the clients. A client that joins later gets the new text,
 * because the events of kind 3 stay in the event list for the whole round. The next round loads the news again. */
int newspaper_set_line(int line, const char *text)
{
    char *dest;
    size_t length;
    if (line < 0 || line > g_newspaper.num_lines)
        return NEWS_BAD_LINE;
    if (line >= NEWS_MAX_LINES)
        return NEWS_FULL;
    length = strnlen(text, NEWS_TEXT_MAX + 1);
    if (length > NEWS_TEXT_MAX)
        return NEWS_BAD_TEXT;
    dest = (char *)g_newspaper.lines + line * 256;
    memset(dest, 0, 256);
    memcpy(dest, text, length);
    if (line == g_newspaper.num_lines)
        g_newspaper.num_lines++;
    newspaper_send_line(line);
    return NEWS_OK;
}

/* Copies a news line, at most NEWS_TEXT_MAX characters and a zero. Returns 0 for a line that does not exist. */
int newspaper_get_line(int line, char out[NEWS_TEXT_MAX + 1])
{
    const char *text;
    size_t length;
    if (line < 0 || line >= g_newspaper.num_lines || line >= NEWS_MAX_LINES)
        return 0;
    text = (const char *)g_newspaper.lines + line * 256;
    length = strnlen(text, NEWS_TEXT_MAX);
    memcpy(out, text, length);
    out[length] = 0;
    return 1;
}

void newspaper_load(void)
{
    Newspaper *np = &g_newspaper;
    int i, n;
    loadtextfile("readme.txt");
    np->num_lines = 0;
    while (g_parser.pos < g_parser.size && np->num_lines < 1013) {
        cs_getline((char *)np->lines + np->num_lines * 256, 0x7c);
        np->num_lines++;
    }
    strcpy((char *)np->lines + np->num_lines * 256, "Top 10 list");
    np->num_lines++;
    n = g_numofaccounts;
    g_radix.count = n;
    for (i = 0; i < n; i++)
        g_radix.keys[i] = 0x8fffffffu - (uint32_t)g_accounts[i].money;
    csradixsort();
    for (i = 0; i < 10; i++) {
        if (i < g_numofaccounts) {
            Account *account = &g_accounts[g_radix.order[i]];
            sprintf(g_newspaper_line, "%d. %s   $", i + 1, account->name);
            sprintf_comma(g_newspaper_money, account->money);
            strcat(g_newspaper_line, g_newspaper_money);
            strcpy((char *)np->lines + np->num_lines * 256, g_newspaper_line);
            np->num_lines++;
        }
    }
}
