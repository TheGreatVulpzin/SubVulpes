/* Parse bounded Standard MIDI Files and convert time with integer arithmetic. */
#include "midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { IGNORE, NOTE, PROGRAM, TEMPO };
typedef struct {
    uint64_t tick;
    uint32_t order, tempo;
    unsigned char kind, channel, a, b;
} MidiEvent;

typedef struct {
    const unsigned char *at, *end;
    char *error;
    size_t capacity;
} Reader;

typedef struct {
    MidiEvent *events;
    size_t count, capacity;
} Events;

static int fail(Reader *r, const char *text)
{
    if (r->capacity)
        snprintf(r->error, r->capacity, "%s", text);
    return 0;
}

static uint32_t big32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static unsigned big16(const unsigned char *p)
{
    return (unsigned)p[0] << 8 | p[1];
}

static int byte(Reader *r, unsigned *value)
{
    if (r->at == r->end) /* BOUND: byte */
        return fail(r, "MIDI ends inside an event.");
    *value = *r->at++;
    return 1;
}

static int vlq(Reader *r, uint32_t *value)
{
    unsigned b, count = 0;
    *value = 0;
    do {
        if (++count > 4) /* BOUND: delta-length */
            return fail(r, "MIDI delta or message length exceeds four bytes.");
        if (!byte(r, &b))
            return 0;
        *value = (*value << 7) | (b & 127u);
    } while (b & 128u);
    return 1;
}

static int append(Reader *r, Events *all, MidiEvent event)
{
    if (all->count == JB_EVENT_MAX) /* BOUND: event-count */
        return fail(r, "MIDI has more than 200000 events.");
    if (all->count == all->capacity) {
        size_t capacity = all->capacity ? all->capacity * 2 : 1024;
        MidiEvent *events = realloc(all->events, capacity * sizeof *events);
        if (!events)
            return fail(r, "Not enough memory for MIDI events.");
        all->events = events;
        all->capacity = capacity;
    }
    event.order = (uint32_t)all->count;
    all->events[all->count++] = event;
    return 1;
}

static int track(Reader *r, Events *all)
{
    uint64_t tick = 0;
    unsigned running = 0;
    int ended = 0;
    while (r->at < r->end) {
        MidiEvent event = {0};
        uint32_t delta, length;
        unsigned status, a = 0, b = 0;
        if (!vlq(r, &delta) || !byte(r, &status))
            return 0;
        tick += delta;
        event.tick = tick;
        if (status < 128) {
            if (!running) /* BOUND: running-status */
                return fail(r, "MIDI running status has no previous channel status.");
            --r->at;
            status = running;
        }
        if (status < 240) {
            running = status;
            event.channel = (unsigned char)(status & 15u);
            if (!byte(r, &a))
                return 0;
            if ((status & 240u) != 192u && (status & 240u) != 208u && !byte(r, &b))
                return 0;
            if (a > 127 || b > 127) /* BOUND: channel-data */
                return fail(r, "MIDI channel data exceeds 127.");
            event.a = (unsigned char)a;
            event.b = (unsigned char)b;
            if ((status & 240u) == 144u && b)
                event.kind = NOTE;
            else if ((status & 240u) == 192u)
                event.kind = PROGRAM;
        } else if (status == 255) {
            running = 0;
            if (!byte(r, &a) || !vlq(r, &length))
                return 0;
            if (length > (size_t)(r->end - r->at)) /* BOUND: meta-length */
                return fail(r, "MIDI meta message exceeds its track.");
            if (a > 127) /* BOUND: meta-type */
                return fail(r, "MIDI meta type exceeds 127.");
            if (a == 81) {
                if (length != 3) /* BOUND: tempo-length */
                    return fail(r, "MIDI tempo needs three bytes.");
                event.kind = TEMPO;
                event.tempo = (uint32_t)r->at[0] << 16 | (uint32_t)r->at[1] << 8 | r->at[2];
                if (!event.tempo) /* BOUND: tempo-zero */
                    return fail(r, "MIDI tempo must be positive.");
            } else if (a == 47) {
                if (length || r->at != r->end) /* BOUND: end-track */
                    return fail(r, "MIDI end-of-track must be empty and last.");
                ended = 1;
            }
            r->at += length;
        } else if (status == 240 || status == 247) {
            running = 0;
            if (!vlq(r, &length))
                return 0;
            if (length > (size_t)(r->end - r->at)) /* BOUND: system-length */
                return fail(r, "MIDI system message exceeds its track.");
            r->at += length;
        } else {
            return fail(r, "MIDI contains an unsupported system status.");
        }
        if (!append(r, all, event))
            return 0;
    }
    if (!ended) /* BOUND: missing-end */
        return fail(r, "MIDI track has no end-of-track event.");
    return 1;
}

static int compare(const void *left, const void *right)
{
    const MidiEvent *a = left, *b = right;
    if (a->tick != b->tick)
        return a->tick < b->tick ? -1 : 1;
    return a->order < b->order ? -1 : a->order != b->order;
}

static int convert(Reader *r, Events *all, unsigned division, JbSong *song)
{
    unsigned char programs[16] = {0};
    uint64_t previous = 0, ticks = 0, remainder = 0;
    uint64_t denominator = (uint64_t)division * 1000000u;
    uint32_t tempo = 500000;
    size_t i, count = 0;
    qsort(all->events, all->count, sizeof *all->events, compare);
    for (i = 0; i < all->count; ++i)
        count += all->events[i].kind == NOTE;
    if (count) {
        song->notes = malloc(count * sizeof *song->notes);
        if (!song->notes)
            return fail(r, "Not enough memory for MIDI notes.");
    }
    for (i = 0; i < all->count; ++i) {
        const MidiEvent *e = &all->events[i];
        /* A gap is at most one four-byte delta. The bounded product fits uint64_t. */
        uint64_t scaled = (e->tick - previous) * tempo * 60u + remainder;
        ticks += scaled / denominator;
        remainder = scaled % denominator;
        previous = e->tick;
        if (e->kind == TEMPO) {
            tempo = e->tempo;
        } else if (e->kind == PROGRAM) {
            programs[e->channel] = e->a;
        } else if (e->kind == NOTE) {
            JbNote *note = &song->notes[song->count++];
            JbChannel *channel = &song->channels[e->channel];
            note->tick = ticks;
            note->channel = e->channel;
            note->key = e->a;
            note->velocity = e->b;
            note->program = programs[e->channel];
            if (!channel->notes) {
                channel->first_program = note->program;
                channel->low = channel->high = e->a;
            }
            if (e->a < channel->low)
                channel->low = e->a;
            if (e->a > channel->high)
                channel->high = e->a;
            ++channel->notes;
        }
    }
    song->end_tick = ticks;
    return 1;
}

int jb_midi_parse(const unsigned char *data, size_t size, JbSong *song, char *error, size_t capacity)
{
    Reader r = { data, data, error, capacity };
    Events all = {0};
    unsigned format, tracks, division, i;
    uint32_t header;
    int ok = 0;
    if (capacity)
        error[0] = 0;
    memset(song, 0, sizeof *song);
    if (size > JB_FILE_MAX) /* BOUND: file-size */
        return fail(&r, "MIDI file exceeds 1 MB.");
    if (!data || size < 14) /* BOUND: header-size */
        return fail(&r, "MIDI header is truncated.");
    r.end = data + size;
    if (memcmp(data, "MThd", 4)) /* BOUND: header-tag */
        return fail(&r, "MIDI needs an MThd header.");
    header = big32(data + 4);
    if (header < 6 || header > size - 8) /* BOUND: header-length */
        return fail(&r, "MIDI header length is invalid.");
    format = big16(data + 8);
    tracks = big16(data + 10);
    division = big16(data + 12);
    if (!tracks || tracks > JB_TRACK_MAX) /* BOUND: track-count */
        return fail(&r, "MIDI needs 1 to 64 tracks.");
    if (format > 1 || (format == 0 && tracks != 1)) /* BOUND: format */
        return fail(&r, "MIDI supports format 0 with one track or format 1.");
    if (!division || (division & 32768u)) /* BOUND: division */
        return fail(&r, "MIDI needs a positive PPQ division. SMPTE time is unsupported.");
    r.at = data + 8 + header;
    for (i = 0; i < tracks; ++i) {
        Reader part;
        uint32_t length;
        if ((size_t)(r.end - r.at) < 8) { /* BOUND: track-header */
            fail(&r, "MIDI track header is truncated.");
            goto done;
        }
        if (memcmp(r.at, "MTrk", 4)) { /* BOUND: track-tag */
            fail(&r, "MIDI needs an MTrk chunk for each track.");
            goto done;
        }
        length = big32(r.at + 4);
        r.at += 8;
        if (length > (size_t)(r.end - r.at)) { /* BOUND: track-length */
            fail(&r, "MIDI track length exceeds the file.");
            goto done;
        }
        part = r;
        part.end = r.at + length;
        if (!track(&part, &all))
            goto done;
        r.at = part.end;
    }
    if (r.at != r.end) { /* BOUND: trailing-data */
        fail(&r, "MIDI has data after the declared tracks.");
        goto done;
    }
    ok = convert(&r, &all, division, song);
done:
    free(all.events);
    if (!ok)
        jb_song_free(song);
    return ok;
}

void jb_song_free(JbSong *song)
{
    free(song->notes);
    memset(song, 0, sizeof *song);
}
