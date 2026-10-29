# Game sounds and pitch

Status: first findings on 2026-10-28. The goal is a plugin that plays music with the game sounds. This page records what we know about the sounds and how to measure the rest.

## How a sound plays

A plugin calls `sound(index, position, volume, pitch)`. The client plays the sound with that index at the position. The pitch is a multiplier of the playback rate, so the frequency that you hear is the base frequency of the sound times the pitch. One octave up doubles the pitch.
To play a note of frequency F with a sound whose base frequency is B, use `pitch = F / B`.
The API accepts a pitch from 0.1 to 4.0. Whether the client limits it to a smaller range is still open (see the tests below).

## The sound ids

The client loads its sounds in a fixed order, and the server uses the same ids (0 to 46). The names below come from the file names in the client program (`subrosa.exe`) and from the constants of `game_setup.c` (`crash=20`, `humanhit=22`, `phone_ring=27`, `digit=28..37`, `magazine=38`, `gear=40`).

| Id | Sound | Note |
| --- | --- | --- |
| 20, 21 | `carcrash01`, `carcrash02` | metallic. The first one has a clear tone near 1068 Hz |
| 22, 23 | `bullethitbody01`, `bullethitbody02` | noise |
| 24, 25 | `bullethitmetal01`, `bullethitmetal02` | noise |
| 26 | `glassbreak01` | noise |
| 27 | `phonering01` | a pure tone at 2005 Hz (B6 plus 26 cents), steady to 2 cents |
| 28 to 37 | the digits of the phone | not WAV files. Probably tones made by the client. To measure |
| 38 | `reload01` | noise |
| 39 | `shell_hit_cement` | a short tone at 2207 Hz |
| 40 | `gearshift01` | noise |
| 41 | `helicopter02` | noise |
| 0 to 19 | `44magnum`, `submachinegun01`, `rifle01`, `4cylinder`, `tiresqueal`, `ricochet01` to `ricochet08`, and 7 sounds that are not WAV files | the order inside this block is not known |
| 42 to 46 | `train01` and 4 more | not known |

Only the phone ring is a clean tone. The ten digit sounds are the best hope for a set of instruments.

## Tools

- `tools/audio/analyze_sounds.py CLIENT_SOUND_FOLDER` measures the base frequency of each WAV file (autocorrelation) and says how tonal it is. The output is a table of numbers in `build/audio/sounds.csv`.
- `tools/audio/match_probe.py SERVER_LOG PROBE_LOG CLIENT_SOUND` matches each sound that the test room sent to the sound that the client played, and compares the pitch that was sent with the pitch that the client set.
- The probe is a pass-through `OpenAL32.dll` for the client. It logs every buffer that the client loads (and saves the audio to a folder), and every sound that plays with its pitch and gain. It stays outside the repository (`build/audio/openal_probe`), because the project rules keep hook libraries out. It is for a local test client only.

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
