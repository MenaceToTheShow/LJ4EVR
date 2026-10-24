# Audio Analyzer and audio trig

LJ2 shipped its Audio Analyzer as a separate program, `LJSound.exe`. LJ4EVR builds it into the app. Audio is captured
through SDL3 and analysed with integer-only code, so the same input gives the same trigs on Windows, macOS and Linux.
Research note 05 s9.1 and the CHM topic "Using audio input" describe the LJ2 behaviour this follows. Gates W5-AUD-1..6
are in `docs/w5-gates.md`.

## Turning it on

Setup > Hardware Setup > Audio: set *Audio Analyzer (via soundcard)* to *Enabled* and pick the input in the device list.
The list starts with *Default device*, which is the system's default recording device. LJ2 called this the *Microsoft
Sound Mapper*. Click OK. The setting is saved to `Hardware.Ini [AUDIO] Enabled` and `Device`, and the capture opens
immediately. A fresh install has it off (W5-X-OFF): no device is opened until you enable it. The CD-Audio and Winamp
rows on the same page stay greyed (fidelity I5).

&Audio > Start Audio Analyzer opens the Analyzer window. You can close the window and capture keeps running, so trigs
still reach the cue. Only Hardware Setup turns capture off.

## The window

The window is LJ2's `TLJSoundApp` form: the same captions, geometry and menus, with Martin's typos kept ("Aggresivness",
"Alway put Audio Analyzer in front").

| Control | What it does |
|---|---|
| Left scroll bar (Gain) | Input gain, -20 to +20 dB (`Gain: 6,9dB` in the hint). It scales the graph and the noise-threshold comparison only. The level-ratio test is unchanged |
| Left fader (Decay) | Minimum time between two trigs: 24 ms per step, 24 to 600 ms ("approx. 0.05 to 0.6 seconds") |
| Middle fader (Noise cut-off level) | The noise threshold within the range set in Trigger > Noise level (Low / Medium / High). You can also drag the blue line in the Time-line view |
| Right fader (Aggresivness) | Trig sensitivity: left = factor 10, right = factor 2. A block trigs when its level is more than factor x the running average of earlier blocks |
| Graph | View > Show input: None, Real time (one 11.6 ms block; yellow near clipping, red when clipped) or Time-line (the last 5.8 s; green level, yellow trig lines, blue threshold). The red/green BPM figure under it is the smoothed time between trigs |
| Spectrum + equalizer | Shown while Trigger > Use Equalizer is on: 16 bands of a 512-point FFT. Drag the yellow curve to weight a band (0 to 2x). Trigger > Set Default > Equalizer flattens it |
| Status bar | Clip indicator (yellow / red), the Audio Trig indicator (blue), and the input name or the reason the device did not open |
| Device list | The same choice as Hardware Setup. Changing it reopens the capture |

Analyzer > Auto Save Changes writes the settings to `Hardware.Ini [AUDIO]` when the window closes, as LJ2's hint says
("Save Equalizer and volume settings on exit"). Without it, changes last until LJ4EVR exits.

## Where trigs go

Each trig posts the same engine command as the *Emulate Audio Trig* hotkey and the DMX-in function of that name
(`LJ_CMD_TRIG LJ_RUN_ALL / LJ_TRIG_AUDIO_EVENT`). Two kinds of runner act on it:

- cue slots in Audio trig mode, which advance one scene every *Audio-Trigs / Trig* trigs (Loop Options);
- BG cue slots in `mode=audio`, which advance one scene per trig.

Trig times come from the count of samples analysed since the capture opened (44100 per second), not from the
clock at the moment the UI loop polls, so a late poll does not move a trig or shorten the decay (W6 LORD_ENGINE-7).
The status bar's *Trig* panel flashes blue on each analyzer trig. Replay scenario
`tests/engine/replay/w5/audio_trig.lj4r` (CTest `replay_w5_audio`) pins this behaviour.

Four remote inputs drive the same two analyzer settings (W5-AUD-6). For Gain, the DMX-in function *Audio Analyzer,
Gain* (level 128 = 0 dB) and the Fingers jog *Audio Gain*. For the noise threshold, the DMX-in function *Audio
Analyzer, Noise Threshold* (level = threshold) and the Fingers jog *Min Trglvl*. The window follows them.

## Permissions

- **macOS** asks for microphone access the first time capture opens. The bundle carries `NSMicrophoneUsageDescription`
  (`packaging/macos/Info.plist.in`, checked by `release.yml`). If you answer *Don't Allow*, the open fails. The log
  says why, the main status bar shows *Audio-in failed* and the Analyzer's status line gives SDL's reason. Nothing
  else changes. To allow access later: System Settings > Privacy & Security > Microphone.
- **Windows** opens the device even when Settings > Privacy & security > Microphone blocks desktop apps. The input is
  then silent: no level and no trigs. If the graph stays flat, check that switch first.

## Numbers (PROPOSAL rig W5-AUD-2 / -4)

The help leaves these open. Each is tagged where it is defined (`src/io/lj_audio.h`, `src/windows/lj_audio_form.c`).

| Setting | Value |
|---|---|
| Capture format | 44100 Hz, mono, signed 16-bit (SDL converts from the device format) |
| Analysis block | 512 samples (11.6 ms); level = mean absolute sample, or with the equalizer the EQ-weighted sum of the band magnitudes |
| Running average | (7 x average + level) / 8 per block |
| Threshold ranges | noise x 2 / x 8 / x 32 for Low / Medium / High (up to 510 / 2040 / 8160 of 32767) |
| Clip | yellow at 90 % of full scale, red at full scale |
| Defaults | the DFM fader positions: decay 13 (312 ms), aggressiveness 150 (factor 6.0), gain 0 dB, noise 16 at Medium, Real-time view |
| FFT | Hann window, radix-2, 1/2 scale per stage, magnitude max + min / 2; bands geometric over bins 1..256 (`tools/gen_vectors.py --audio`) |

`Hardware.Ini [AUDIO]` uses the key names from `LJSound.exe`'s `LJSound.ini` (`UseFFT`, `NoiseLevelSwitch`,
`NoiseLevelValue`, `TrigDecay`, `TrigAggr`, `Gain`, `EqualizerBand1..16`, `ShowInput`, `TriggerFaders`, `SoundCard`,
`StayOnTop`, `RunMinimized`, `AutoSaveChanges`, `PassKeys`). The values are in LJ4EVR's own units: ms, factor x 10,
percent, 0..255. LJ2 never documented its units, and LJ4EVR does not read LJ2's `LJSound.ini`.

## Differences from LJ2

- One process. *Pass Keypresses to LightJockey* works as in LJ2: off (the default), a hotkey pressed while the
  Analyzer has focus stays with it; on, LightJockey's hotkeys act on it (ME-182). *Run Minimized* keeps the Analyzer
  closed at startup; off, it opens with the show while audio is enabled (no minimized state).
- The &Audio menu shows only while the Audio Analyzer is enabled in Hardware Setup (the DFM's `Visible = False`, ME-181).
- *Stay On Top* takes effect the next time the window opens.
- The device list's first entry is *Default device*, not *Microsoft Sound Mapper*.
- Labels `Label2` / `Label3` of the form are hidden. No screenshot shows them in use.

## Tests

| CTest | What it checks |
|---|---|
| `test_audio` | The analyzer on synthetic PCM from `tests/io/audio_vectors.h`: a 120 BPM click train = 2 trigs/s, silence 0, noise under the threshold 0, decay and sensitivity extremes, gain, FFT bands and EQ, clip, the capture ring, `[AUDIO]` round trip and clamping, W5-AUD-6 inputs |
| `test_audio_form` | The window and menus headless, the Hardware Setup page with a fake device list, enable persisting, a denied device, `audio_inject`, the status-bar Trig, goldens `W5AU_ANALYZER` and `W5AU_ANALYZER_REALTIME` |
| `test_audio_sdl` | The SDL3 capture on SDL's dummy driver (`SDL_AUDIO_DRIVER=dummy`): list, open, samples flow, 0 trigs on silence, fallback for an absent device, close |
| `replay_w5_audio` | Audio trigs on an Audio-Trigs / Trig 2 slot, a BG `mode=audio` slot and an auto slot, then 3 trigs per scene |
| `gen_vectors_audio_check` | `src/io/lj_audio_gen.c` and `tests/io/audio_vectors.h` match the generator |
