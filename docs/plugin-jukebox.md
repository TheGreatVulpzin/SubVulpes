# MIDI jukebox 0.2.0

`sv.jukebox` plays MIDI files with the 47 existing game sound IDs.
It is an optional native SDK example with `read`, `write` and `config` access.
It adds no core, API or launcher code. It is absent from the official server package.
Chat commands require an administrator. The console cannot start music.

## Build and install

Run these commands from an x86 MSVC prompt:

```text
cmake --build build/fresh
cmake --build build/fresh --target sv-jukebox-package
```

The DLL folder is `build/fresh/plugins/sv.jukebox`.
The package is `build/fresh/packages/sv.jukebox-0.2.0-native.svplugin`.
The package contains only `jukebox.dll` and `plugin.ini`.
Obey the [SDK installation steps](sdk-first-steps.md).

An extracted SDK also supports this plugin:

```text
plugins\jukebox\build.bat build\jukebox-sdk C:\SubVulpes-sdk
cmake -S plugins/jukebox -B build/jukebox-sdk -DSV_SERVER_EXE=C:\Test-server\subvulpes_server.exe
cmake --build build/jukebox-sdk --target sv-jukebox-package
```

## Start music

1. Join as an administrator with a live body.
2. Type `/jb demo` for the built-in pattern.
3. Put your MIDI files in `user/jukebox/`.
4. Type `/jb list`, then `/jb play NAME`.
5. Type `/jb channels` to choose the parts that play.

`/jb` and `/jukebox` are aliases. A sound already sent plays to its end.
Stop, mute and note off cannot stop that sound.
The demo uses four ping notes and two drum notes across 45 ticks.
Its ping root is 60 so the tails finish between notes.
File songs use the measured root of 109 for the default ping voice.

Names need 1 to 48 ASCII letters, digits, underscores or hyphens.
An optional `.mid` suffix is accepted. Paths, spaces and Windows device names are rejected.
Matching ignores case and checks exact names, prefixes, then parts anywhere in the name.
For example, `play moon` or `play sonata` can select `moonlight-sonata.mid`.
An exact `cirno` wins over `cirno1`. An ambiguous name starts no new song.
The answer lists at most eight matches and says when more exist.
`list` shows at most 128 files. Song matching searches all eligible files.
A bad song leaves the current performance intact.

## Commands

Use each command after `/jb` or `/jukebox`.

| Command | Effect |
| --- | --- |
| `list`, `play NAME`, `demo`, `stop` | List, start or stop music. |
| `status` | Show mode, tempo, speed, key, master, limits and counters. |
| `sounds` | List every sound ID, name, kind, root, tail and gain. |
| `try SOUND [ROOT]` | Play 12 chromatic notes, one every 15 ticks. |
| `mode` | List modes and show the active mode. |
| `mode voices` | Use channel voices, with ping as the default. |
| `mode phone` | Use the old nearest phone bank. |
| `mode keypad` | Use digit tones at pitch 1.0. |
| `mode fixed SOUND` | Use one sound for all melody channels without overrides. |
| `voice N SOUND [ROOT]` | Override channel N with a sound ID or name and an optional root, 0 to 127. |
| `voice N default` | Remove that override and use the mode. |
| `channels` | Show used channels, instruments, note counts and voices. |
| `t N [N ...]` | Toggle the listed channels. |
| `on N [N ...]`, `off N [N ...]` | Turn the listed channels on or off. |
| `solo N [N ...]`, `all` | Select only those channels, or turn all channels on. |
| `mute N`, `unmute N` | Keep the old mute commands. |
| `out N synth`, `out N drums`, `out N off` | Select melody, drum or disabled output. |
| `transpose N SEMITONES` | Move one channel by -36 to 36 semitones. |
| `mono N 0`, `mono N 1` | Allow overlap, or skip notes until the previous tail ends. |
| `bpm N`, `bpm file` | Use constant BPM, 20 to 300, or restore the file map at speed 100. |
| `speed P` | Scale the file tempo map by 25 to 400 percent. |
| `key SEMITONES` | Move all melody channels by -48 to 48 semitones. |
| `vol N P` | Set channel volume from 0 to 200 percent. |
| `master P` | Set master volume from 0 to 200 percent. |
| `save` | Save a preset for the current file song. |
| `scale` | Play ring, ten digits, nearest phone, then drums, as in version 0.1.0. |
| `chord N` | Queue 1 to 32 notes for a sound-call test. |

Channel numbers are 1 to 16. Channel 10 always plays drums.
Other channels with `out N drums` also use the drum voice and ignore the global key.
An explicit sound override applies in every melody mode.
`voice N thud` remains a pitched melody voice. Use `out N drums` for MIDI drum mapping.
The global key adds to channel transpose. Drum output keeps its channel transpose only.

Changes apply to future notes immediately. Bad arguments change nothing.
A channel line stays within 60 characters. `[x]` means on and `[ ]` means off.
`auto` marks a mute from the channel rule. Long instrument and voice names are shortened.
The busiest three melody channels start on. Ties favor the lower channel number.
Channel 10 stays on. `all` restores every output, including `out N off`.
Replaying the same song keeps channel edits unless a valid preset replaces them.
Loading another song resets channels before it loads that song's preset.

## Modes, pitch and overlap

The default mode is `voices`, with ping ID 39.
A voice uses `pitch = 2^((note - root) / 12)` for tones and noise alike.
Whole octave moves keep pitch within the measured client range, 0.125 to 2.0.
`moved` counts notes that need this fold, once per selected note.

`phone` uses the ring and ten digit tones from `instruments.ini`.
It chooses an in-range member closest to pitch 1.0 before it folds octaves.
The ring has a 50 Hz loudness shake and a repeated pattern. It is a poor melody voice.
Phone mode therefore sounds like a phone. Digit tones contain two frequencies.
`keypad` retains version 0.1.0 behavior: digit tones only, always at pitch 1.0.
It selects one whole-song octave shift with the smallest total distance to the digit frequencies.
Status retains `keypad octave shift` on song start and `keypad_octaves` during playback.
Channel changes, voice overrides and key changes recalculate the active keypad part.
The legacy mode names `nearest`, `ring` and `digit0` through `digit9` remain accepted.

Mono starts on for melody channels and off for channel 10.
A note reserves `ceil(tail_ticks / pitch)` server ticks after a successful sound call.
Thus a ping with tail 14 lasts 56 ticks at pitch 0.25.
Stop and a new song preserve unfinished tails on the same channel.
Notes skipped by mono increment `skipped_mono`. They do not count as limit drops.
Scale, try and chord ignore channel mutes and mono for audition tests.

## Tempo and presets

The file tempo map is the default. `speed` keeps its tempo changes.
`bpm` replaces the whole map. The last tempo command wins.
A live tempo change keeps the musical position and the index of the next event.
Timing uses server callbacks and integer PPQ positions with billionths and a carried division remainder.
Tempo boundaries split a tick's time budget. Boundary conversion rounds by less than one billionth of a server tick.
No wall clock supplies playback time. Repeating commands at the same ticks gives the same calls.
Events within one tick keep their file order.

`save` writes `user/jukebox/NAME.mix` for the full matched song name.
The preset stores every channel's output, mute, voice, root, transpose, mono and volume.
It also stores mode, tempo choice, key, master and automatic mute flags.
A temporary file and atomic rename preserve the previous preset if writing fails.
A stale `.mix.tmp` file blocks saving and gives a message. Remove it after checking it.
The reader accepts version 2, at most 4096 bytes, ASCII and LF or CRLF lines.
It checks each field before it applies any state. A bad or old preset gives `Preset ignored`.
The demo has no disk preset. Partial names always load the preset of the resolved full name.

## Settings

Use the host's plugin settings controls, or edit `user/plugins/sv.jukebox.settings.ini` while the server is stopped.

| Setting | Default and limits |
| --- | --- |
| `mode` | `voices`. Also accepts phone, keypad, fixed SOUND and legacy bank names. |
| `bank` | Empty. A nonempty old value supplies the mode when `mode=voices`. Clear it to use the new default. |
| `auto_channels` | `3`, from 0 to 16. Zero plays all channels. Applies on a new song. |
| `default_bpm` | `0`, for the file map. Otherwise 20 to 300. |
| `default_speed` | `100`, from 25 to 400 percent. A nonzero default BPM takes priority. |
| `default_key` | `0`, from -48 to 48 semitones. |
| `default_master` | `100`, from 0 to 200 percent. |
| `everyone` | `0`: follow the starter. `1`: copy audio at each real player's live body. |
| `max_per_tick` | `8`, from 1 to 32 actual calls, including copies for everyone. |
| `trace` | `0`. Enable MIDI call logs. Demo, try, scale and chord always log calls. |

Live defaults apply on the next song load. Mode and delivery settings apply immediately.
Invalid live settings retain the last valid settings. Invalid startup settings prevent activation.
A valid preset takes priority over song defaults.
With `everyone=0`, loss of the starter's live body stops future notes.
Each copy is spatial audio. Nearby listeners can hear several copies.
Higher velocities win when calls exceed the limit. Ties retain source order, then player order.
Status shows limit drops explicitly. `played` counts successful calls and `failed` counts API failures.
`dropped_notes` counts notes with no budgeted call. `dropped_calls` counts omitted body copies.

## Editable sound tables and volume

The plugin creates `user/jukebox/instruments.ini` and `sounds.ini` only when missing.
It uses exclusive creation. It never replaces owner edits, comments, line endings or old versions.
The source INI files supply the embedded defaults. Both tables need version 1 and at most 16384 ASCII bytes.
Start commands reload both tables. A bad reload leaves the current performance intact.

`sounds.ini` has one row for each ID, 0 to 46:

```ini
[jukebox]
version=1
# id=name,kind,tail_ticks,root_note,gain
39=ping,tonal,14,109,0.79
```

The complete table must contain all 47 rows. Names are unique lowercase letters, digits or underscores, at most 23 characters.
Kinds are `tonal`, `noise` or `silent`. Tails accept 0 to 36000 ticks, roots 0 to 127, and gains 0.05 to 1.0.
Tonal roots come from rounded measured frequencies. Noise roots use 60.
Silent IDs remain available but produce no measured client output.
The measurements come from [audio analysis](audio-analysis.md).
Engine, stream and silent IDs lack loudness measurements, so their gain starts at 1.0.

The sound volume is `min(1, velocity / 127 * sound_gain * channel_volume / 100 * master / 100)`.
Zero volume sends no sound call. Measured gains reduce louder sounds toward -20 dB RMS.
Quiet sounds retain gain 1.0. No pitch-dependent loudness curve applies.
High notes can sound louder than low notes. Use `vol` to fine tune each channel.

`instruments.ini` retains the phone frequencies, drum sound and 128 drum pitches from version 0.1.0.
It requires each sound ID from 0 to 46, each base frequency from 20 to 20000 Hz, and drum pitches from 0.125 to 2.0.
These drum pitches are listening defaults, not measured drum tuning.

## Reader limits and remaining checks

The MIDI reader supports Standard MIDI File formats 0 and 1 with positive PPQ division.
It handles running status, tempo changes, programs and note-on events on all 16 channels.
Note off, velocity-zero notes, controllers and pitch bend do not stop or reshape samples.
Format 2, SMPTE, RIFF wrappers, missing end markers and trailing chunks are unsupported.
Limits remain 1 MB, 64 tracks, 200000 events and four bytes per delta or length.
Every read stays inside the file and track bounds.

Automated checks cover DLL sound calls, channel controls, mono, tempo, key, volume, presets and partial names.
The SDK runner checks the demo twice. Reader checks include proofs with individual bounds removed.
No version 0.2.0 in-game listening, real MIDI listening, client polyphony measurement or jitter measurement ran.
The number of sounds heard at once remains unknown. Server ticks do not prove audio arrival time.
For a client probe, select phone ring, disable mono, set the call limit to 32, and compare separate chord bursts.
Let tails finish between bursts. Compare successful calls with client source starts.
