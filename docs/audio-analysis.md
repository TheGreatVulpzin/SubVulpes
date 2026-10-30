# Game sounds and pitch

Status: measured with the client on 2026-10-29. The goal is a plugin that plays music with the game sounds. This page records what we know about the sounds and how to measure the rest.

## How a sound plays

A plugin calls `sound(index, position, volume, pitch)`. The client plays the sound with that index at the position. The pitch is a multiplier of the playback rate, so the frequency that you hear is the base frequency of the sound times the pitch. One octave up doubles the pitch.
To play a note of frequency F with a sound whose base frequency is B, use `pitch = F / B`.
The API accepts a pitch from 0.1 to 4.0. The client limits the pitch to 2.0 at the top: a pitch of 4.0 played as 2.0. A pitch of 0.125 plays as sent. So a plugin has five octaves, from 0.125 to 2.0 (test 63).

## The sound ids

The server uses the ids 0 to 46. The table below comes from a tour of all 47 ids with the probe (test 62): for each id, the sound that the client played within 100 ms. The constants of `game_setup.c` (`crash=20`, `humanhit=22`, `phone_ring=27`, `digit=28..37`, `magazine=38`, `gear=40`) agree for the ids 27 and up. For the ids 19 to 25 the measured table is one lower than the constants. One tour measured this, so a second tour should confirm it before a plugin depends on it.

| Id | Sound | Note |
| --- | --- | --- |
| 0 | `44magnum` | |
| 1 | `submachinegun01` | the buffer has 39314 bytes, and the file has 39358 bytes (the header is 44) |
| 2 | `rifle01` | |
| 3 to 7, 9 | nothing | the client played no sound in the tour and in test 64 (6 runs for each id). No file for them exists in the sound folder |
| 8 | `4cylinder` | the engine. A new source starts 17 to 73 ms after the call, in 7 of 7 tests with no car running |
| 10 | `tiresqueal` | |
| 11 to 18 | `ricochet01` to `ricochet08` | |
| 19, 20 | `carcrash01`, `carcrash02` | metallic. The first one has a clear tone near 1068 Hz |
| 21, 22 | `bullethitbody01`, `bullethitbody02` | noise |
| 23, 24 | `bullethitmetal01`, `bullethitmetal02` | noise |
| 25 | `glassbreak01` | noise |
| 26 | nothing | |
| 27 | `phonering01` | the ring of a cell phone, 1.47 s long. The main frequency is about 2000 Hz. It has a second harmonic at 4006 Hz (31% of the main peak) and side peaks every 50 Hz, so the loudness shakes 50 times a second. The loudness repeats a pattern every 400 ms, and the last 0.2 s is silent. It is not a pure tone |
| 28 to 37 | the digits of the phone | not WAV files. The client makes ten tones of 16384 bytes at 44100 Hz. Frequencies not measured yet |
| 38 | `reload01` | noise |
| 39 | `shell_hit_cement` | a short tone at 2207 Hz |
| 40 | `gearshift01` | noise |
| 41 | `helicopter02` | noise |
| 42 | a stream | a buffer of 131072 bytes. Perhaps `train01`, which has 882044 bytes. Not proven |
| 43 to 46 | nothing | the client played no sound |

The engine plays in a loop while a car runs and restarts about every 35 ms, so a test inside a car cannot show which id is the engine. Test 64 (`engine_sounds`) sets the bots to 0 and makes a car. The trains stay, because the track system owns them.

The phone ring has one clear main frequency, but it also has a harmonic and a shake of the loudness. The ten digit sounds are the best hope for a set of instruments.

## Tools

- `tools/audio/analyze_sounds.py CLIENT_SOUND_FOLDER` measures the base frequency of each WAV file (autocorrelation) and says how tonal it is. The output is a table of numbers in `build/audio/sounds.csv`.
- `tools/audio/match_probe.py SERVER_LOG PROBE_LOG CLIENT_SOUND` matches each sound that the test room sent to the sound that the client played, and compares the pitch that was sent with the pitch that the client set.
- The probe is a pass-through `OpenAL32.dll` for the client. It logs every buffer that the client loads (and saves the audio to a folder), and every sound that plays with its pitch and gain. It stays outside the repository (`build/audio/openal_probe`), because the project rules keep hook libraries out. It is for a local test client only. The ids of this OpenAL are large numbers (for example 883102208), so the probe keeps them in a hash table.

## How to find the rest

1. Install the probe in a copy or in the client folder (`install.ps1`), and start the test server.
2. In the test room run `/test sound_tour` (62) with `/test go`. It plays the sounds 0 to 46 once, one every 1.5 seconds, and writes a `TOURSOUND` line with the time of each one.
3. Run `/test pitch_octaves` (63) with `/test go`. It plays the phone ring at 0.125, 0.25, 0.5, 1, 2 and 4, twice. Compare the pitch that was sent with the pitch that the client set. If they differ, the client limits the pitch.
4. Close the client and run `match_probe.py`. It gives the file of every id, including the ids with no WAV file (they have a dump).
5. Run `analyze_sounds.py` on the dump folder to get the frequency of the digit tones.

## Limits of a sound as an instrument

- A sound plays once from start to end. A note cannot be held. A bell, a ring or a piano key works. An organ does not.
- The time grid is one tick, about 16 ms.
- The number of sounds that the client plays at the same time is not known.
- The audio files belong to Cryptic Sea. The repository holds numbers only.
