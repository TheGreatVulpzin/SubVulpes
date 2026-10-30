# MIDI jukebox

`sv.jukebox` is an optional native SDK example. It plays music with the game sounds.
It requests only `read`, `write` and `config`. It is absent from `plugins/official.ini` and the server package.
The commands require an administrator in chat. The local console cannot start music.

## Build and install

From the repository, run the normal build, then make the plugin package:

```text
tools\build_hook.bat
cmake --build build/server --target sv-jukebox-package
```

The plugin folder is `build/server/plugins/sv.jukebox`.
The package is `build/server/packages/sv.jukebox-0.1.0-native.svplugin`.
Both use the SDK's `sv_add_plugin` function.
The plugin starts from the SDK template's entry point, command registration and public API lookup.
It adds no core or launcher code.

For a separate build with an extracted SDK:

```text
plugins\jukebox\build.bat build\jukebox-sdk C:\SubVulpes-sdk
cmake -S plugins/jukebox -B build/jukebox-sdk -DSV_SERVER_EXE=C:\Test-server\subvulpes_server.exe
cmake --build build/jukebox-sdk --target sv-jukebox-package
```

The batch file accepts the build folder and SDK root as optional arguments.
Use the existing [SDK install and approval steps](sdk-first-steps.md).
Review native access and the declared capabilities, install the package, then enable `sv.jukebox` on a test server.
The package carries only `jukebox.dll` and `plugin.ini`.

## First music

1. Join the server as an administrator with a live body.
2. Type `/jukebox demo`. It needs no MIDI or audio file.
3. Put your own files in the server's `user/jukebox/` folder.
4. Type `/jukebox list`, then `/jukebox play NAME`.
5. Type `/jukebox stop` to stop future notes.

The demo has four melody notes and two drum notes.
Its first notes play on the next server tick. Its last note follows 45 ticks later.
A sound already sent to a client plays to its end. Stop, note off and mute cannot cut that sound short.

Names have 1 to 48 ASCII letters, digits, underscores or hyphens.
`play` also accepts the `.mid` extension. Paths, spaces and Windows device names are rejected.
The list shows at most 128 playable names. Rename a file if its name does not meet these rules.
A bad file gives an error in chat. It does not replace the current song.

## Commands

| Command after `/jukebox` | Effect |
| --- | --- |
| `list` | List the MIDI files and the built-in demo command. |
| `play NAME` | Read and play a file from `user/jukebox/`. |
| `demo` | Play the built-in short song. |
| `stop` | Stop future sound calls. |
| `status` | Show playback state, bank, tick count, limits and counters. |
| `scale` | Play a C major scale with ring, digit0 through digit9, nearest, then drums. Each step names its voice in chat. |
| `chord N` | Queue 1 to 32 notes for one server tick. The sound-call limit still applies. |
| `channels` | Show each used channel, note count, first program and General MIDI name, note range and current output. |
| `mute N` / `unmute N` | Turn channel 1 to 16 off or on. |
| `solo N [N ...]` | Play only the listed channels. |
| `all` | Turn every channel back on, including an output set to off. |
| `out N synth` / `out N drums` / `out N off` | Set the output voice of a channel. |
| `transpose N SEMITONES` | Move one channel by -36 to 36 semitones. |

Channel changes apply to future notes at once, including during playback.
They last until the song changes. Replaying the same name keeps them. They are not saved to disk.
`unmute`, `solo` and `all` restore the previous voice of an output set to off.
`out` changes the voice but does not undo a separate mute or solo selection.
Scale and chord test the voices directly and ignore the song's channel mutes.

The plugin selects channels, not tracks.
One track can carry several channels. One channel can span several tracks.
Channel 10 starts as drums. The other 15 channels start as synth.
Program numbers display as 1 to 128. MIDI stores them as 0 to 127.
Program names describe the MIDI file. They do not select 128 different game instruments.
The first program is the program in force at that channel's first nonzero-velocity note-on event.

## Settings

Use the existing plugin settings controls.
Alternatively, edit `user/plugins/sv.jukebox.settings.ini` while the server is stopped, then restart it:

```ini
[types]
bank=text
everyone=flag
max_per_tick=int
trace=flag
[settings]
bank=nearest
everyone=0
max_per_tick=8
trace=0
```

| Setting | Values and default |
| --- | --- |
| `bank` | `nearest` by default, `keypad`, `ring`, or `digit0` through `digit9`. |
| `everyone` | `0` by default: follow the starter's live body. `1`: send one sound at each real player's live body. |
| `max_per_tick` | 1 to 32 actual sound calls, default 8. It includes every copy from `everyone`. |
| `trace` | `0` by default. `1` logs each successful MIDI sound call. Demo, scale and chord always log their calls. |

Live changes through the host apply without a restart.
An invalid live bank value logs an error and keeps the previous valid settings.
An invalid bank at startup prevents plugin activation.
With `everyone=0`, a missing starter body stops playback when the next note is due.
Bots and players without live bodies get no separate copy with `everyone=1`.
Each call is spatial audio. Nearby listeners can hear copies sent at several bodies.

When one tick exceeds the call limit, higher velocities win. Equal velocities keep MIDI order, then player order.
The retained calls keep their source order. Copies for an earlier player can use the remaining budget.
`dropped_notes` counts notes with no call. `dropped_calls` counts omitted body copies.
`played` counts successful calls. `moved` counts selected notes that need an octave move, once per note.
Explicit channel transposition does not itself increment `moved`.
`failed` counts rejected API calls. Muted notes do not count as network drops.
Use solo and mute to reduce load and find channels that suit the phone bank.

## Editable instruments

On enable, the DLL creates `user/jukebox/instruments.ini` only if the file does not exist.
It uses an exclusive create operation. It never replaces an existing file or upgrades its version.
Comments, owner values and line endings stay unchanged.
The source file `plugins/jukebox/instruments.ini` owns the embedded defaults.

The owner file begins with `[jukebox]` and `version=1`.
A missing or unsupported version gives an error and leaves the file unchanged.
It must be plain ASCII, at most 16384 bytes.
Each voice needs `sound` from 0 to 46 and `base_hz` from 20 to 20000.
The `drum_pitch` section supplies a pitch from 0.125 to 2.0 for each MIDI note, 0 to 127.
Edit the owner file without a rebuild. Play, demo, scale and chord read it again before they start.
An invalid reload leaves the current performance intact.

The synth bank has ring ID 27 at 1997 Hz and ten digit tones, IDs 28 to 37.
The digit defaults use the lower DTMF frequency from [the audio measurements](audio-analysis.md).
Several digits share that frequency. Ties choose the first member, so the defaults do not select every digit automatically.
The fixed banks and scale let the owner hear every digit.

`nearest` first keeps members whose `note_hz / base_hz` is within 0.125 to 2.0.
It chooses the multiplier nearest to 1.
If every member is outside, it moves the note by whole octaves.
It prefers fewer octave moves, then the multiplier nearest to 1.
Fixed members also fold outside notes by whole octaves.
The measured range has five octave levels: 0.125, 0.25, 0.5, 1 and 2.
The endpoints span four octave intervals per sound.

`keypad` uses digit tones only, always at pitch 1.0.
Before playback, it chooses one octave transposition for the whole active synth part.
It minimizes total note distance in semitones to the ten base frequencies.
Routing and transposition changes recalculate this choice. Status shows the octave shift.
DTMF has two peaks, so the heard pitch can differ from the assigned note.
This mode tests that effect. It does not turn the tones into pure notes.

Drums use ID 21, the measured `bullethitbody01` thud.
Kick defaults are low, snares and toms are higher, and hats are highest.
These pitches are starting values for listening tests, not measurements of drum tuning.
The drum scale uses the kick entry as a root and applies C major intervals.

## Timing and reader limits

The parser supports Standard MIDI File formats 0 and 1 with positive PPQ timing.
It supports running status, tempo changes, note on/off and program changes on all 16 channels.
It ignores note off, velocity-zero note on, controllers, pitch bend and bounded system or unknown meta messages for playback.
Format 2, SMPTE timing, RIFF wrappers, missing end markers and extra chunk data are unsupported.
The [Standard MIDI Files specification](https://midi.org/standard-midi-files) defines the file messages.

The parser merges tracks by MIDI tick, then file order.
It carries fractional time through the tempo map with integer arithmetic at 60 server ticks per second.
It never reads a clock. Playback advances once per tick callback.
In traces, `at` is this playback tick. `tick` is the host game tick, which stays zero in the SDK runner.
The scenario checks playback ticks. Its Python check also checks sound order and pitch within 0.000002.
Note frequency uses equal temperament with A4 at 440 Hz.
Velocity divided by 127 supplies volume. A sample cannot be held or stopped.

Limits are 1 MB (1048576 bytes), 64 tracks, 200000 events and four bytes per delta or message length.
All events count, including ignored messages and end markers.
Every byte and chunk read stays within its file or track.
Meta and system payloads must fit. Running status needs a preceding channel status.
Tempo needs three bytes and a positive value. Channel data stays below 128.
The reader rejection cases and their removal proofs are in `tests/jukebox_midi_test.py` and `tests/jukebox_removal_test.py`.

## Listening and checks still needed

For the polyphony probe, select `ring`, set `everyone=0` and set `max_per_tick=32`.
Run `chord 1`, `chord 2`, `chord 4`, `chord 8`, `chord 16` and `chord 32` as separate tests.
Let sample tails finish between tests.
Save the `JUKEBOX_SOUND` lines and the existing audio probe's buffer and source-start records.
Match the ring buffer and count the client starts in each burst. Compare them with the successful server calls.
The existing test-room log matcher has no jukebox parser. No audio tool changes are part of this work.

No client listening test, real MIDI listening test, client polyphony measurement or jitter measurement ran.
The tick grid predicts scheduling only. It does not prove audio arrival time or the number of sounds heard at once.
