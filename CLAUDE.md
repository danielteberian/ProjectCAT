# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Project Cat** is a hardware synthesizer built on a Teensy 4.1 microcontroller. It's a complete audio workstation featuring real-time synthesis, effects processing, and WAV recording to SD card.

## Technical Specifications

### Hardware
- **MCU**: Teensy 4.1 (ARM Cortex-M7 @ 600MHz)
- **RAM**: 1MB DTCM (fast internal), 8MB PSRAM (external, not used for audio)
- **Storage**: MicroSD card slot (SDIO interface, built-in)
- **Audio DAC**: PCM5102 I2S (16-bit, 44.1kHz, slave mode)
- **Amplifier**: LM386 audio amplifier
- **Display**: SSD1306 128x64 OLED (I2C, address 0x3C)
- **Development**: Arduino IDE 2.3.0+ with Teensyduino

### Audio System
- **Sample rate**: 44.1kHz (CD quality)
- **Bit depth**: 16-bit signed integer output
- **Block size**: 128 samples per block (2.9ms @ 44.1kHz)
- **Memory allocation**: 70 blocks total (17.5KB)
  - Synth voice: ~10-15 blocks (idle)
  - RecordQueue: ~53 blocks (when recording)
  - Headroom: ~5-10 blocks
- **CPU usage**: ~10-20% (single voice, typical)
- **Latency**: <3ms (one audio block)

### Signal Chain (DSP Order)
1. Source oscillator (SAW/SINE/SQUARE/TRI/NOISE/KARPLUS-STRONG)
2. Optional FM modulation (ratio + depth adjustable)
3. Optional LFO modulation (rate + depth adjustable)
4. Sample-rate reducer (lo-fi decimation)
5. Bitcrusher (1-16 bit depth)
6. XOR distortion (digital grit)
7. Wavefolder (reflective, 6-iteration clamp)
8. Tanh waveshaper (drive control, 1-16× gain)
9. Resonator (pitch-tracking 2-pole IIR filter)
10. TPT state-variable lowpass (Cytomic topology, cutoff + resonance)
11. AD/ASR envelope (attack/decay or attack/sustain/release)
12. Output gain (final level control)

### Control Interface
- **4 potentiometers**: Cutoff, Drive, Decay, Pitch (12-bit ADC, exponentially smoothed)
- **2 buttons**: Trigger (D8), Record (D9)
- **1 rotary encoder**: Page scroll + effects/sound selection (detent-filtered quadrature)
- **OLED display**: 6 pages (LIVE/TONE/ENV/LOFI/REC/SCOPE) @ 10fps

### Sound Engine
- **32 built-in presets**:
  - 6 waveforms: SAW, SINE, SQUARE, TRI, NOISE, KARPLUS-STRONG
  - 10 drums: KICK, SNARE, HIHAT, CLAP, TOM, COWBELL, RIMSHOT, CYMBAL, WOODBLOCK, SHAKER
  - 12 synths: BASS, LEAD, PAD, BELL, PLUCK, ORGAN, BRASS, STRINGS, ARP, CHOIR, RHODES, WOBBLE
  - 4 special FX: LASER, BLEEP, ZAP, GLITCH
- **6 effects modes**: OFF, BITCRUSH, DISTORT, DELAY, FM, LFO
- **Real-time parameter control**: All pots override presets instantly
- **Polyphony**: Monophonic (1 voice)

### Recording System
- **Format**: WAV (16-bit PCM, 44.1kHz mono)
- **Storage**: SD card (FAT32 or exFAT)
- **File naming**: Auto-incrementing `/sample000.wav`, `/sample001.wav`, etc.
- **Memory management**: Lazy queue init (only allocates when recording)

## Build and Upload Commands

```bash
# Open the main sketch in Arduino IDE
open ProjectCat_Teensy.ino

# Or use arduino-cli if installed
arduino-cli compile --fqbn teensy:avr:teensy41 ProjectCat_Teensy.ino
arduino-cli upload -p /dev/cu.usbmodem* --fqbn teensy:avr:teensy41 ProjectCat_Teensy.ino
```

**Arduino IDE Settings**:
- Board: Teensy 4.1
- USB Type: Serial (or Serial + MTP Disk for SD card access)
- CPU Speed: 600 MHz
- Optimize: Fastest

**Monitor Serial Output**:
```bash
# Arduino IDE: Tools → Serial Monitor (115200 baud)
# Or use screen
screen /dev/cu.usbmodem* 115200
```

**Emergency Recovery** (if Teensy won't boot):
1. Press and hold white button on Teensy for 15 seconds to enter bootloader
2. Upload sketch while in bootloader mode
3. Check Serial Monitor for boot step messages (7 steps during startup)

## Audio Architecture

### Custom Audio Object: AudioSynthProjectCat

The core synthesis engine is **not** a standard Teensy Audio Library object. It's a custom `AudioStream` subclass implementing a complete DSP signal chain.

**Key characteristics**:
- Inherits from `AudioStream` (0 inputs, 1 output)
- Renders audio in `update()` callback (128 samples per block @ 44.1kHz)
- Thread-safe parameter setting via atomic writes
- Completely replaces the ESP32 FreeRTOS audio task from the original Project Cat

**Signal chain** (fixed order, executed in `renderSample()`):
1. **Source**: OSC (saw/sine/square/tri/noise) or Karplus-Strong string model
2. **Modulation**: Optional FM + LFO (both disabled by default)
3. **Sample-rate reducer**: Lo-fi decimation effect
4. **Bitcrusher**: Reduces bit depth (default: 16-bit, no crushing)
5. **XOR distortion**: Digital grit
6. **Wavefolder**: Reflective wavefolding
7. **Tanh waveshaper**: Analog-style saturation (drive control)
8. **Resonator**: Tuned 2-pole feedback filter (pitch-tracking)
9. **TPT state-variable filter**: Lowpass with cutoff + resonance (Cytomic topology)
10. **AD/ASR envelope**: Attack-decay or attack-sustain-release
11. **Output gain**: Final volume control

### Thread Safety Rules

Parameters are read in the audio interrupt (`update()`) and written from `loop()`. The Cortex-M7 guarantees atomic 32-bit aligned writes, so individual setters are safe. **However**:

- **For multi-parameter updates** (e.g., note trigger + pitch change), wrap in:
  ```cpp
  AudioNoInterrupts();
  voice.setPitch(440.0f);
  voice.noteOn();
  AudioInterrupts();
  ```
- Never add mutexes or critical sections inside `update()` — it runs in a DMA interrupt
- Keep `update()` deterministic and fast (<20% CPU usage per voice)

### CRITICAL: Audio Memory Management

**Problem**: The Teensy Audio Library uses a fixed pool of memory blocks (`AudioMemory(N)`) shared by all audio objects. `AudioRecordQueue` consumes ~53 blocks when active, which can starve the synth voice and cause distortion/dropouts.

**Solution**: Lazy queue initialization (implemented as of 2026-06-24):
- `recQueue.begin()` is called ONLY when starting recording (not at boot)
- `recQueue.end()` is called when stopping recording, freeing ~53 blocks immediately
- Without recording: synth uses ~10-15 blocks (plenty of headroom for rapid note triggers)
- With recording: total usage ~60-65 blocks out of 70 (safe)

**Configuration**:
```cpp
AudioMemory(70);           // Total pool: 70 blocks
recQueue.begin();          // Called only in startRecording()
recQueue.end();            // Called in stopRecording() to free blocks
```

**Memory monitoring**: Serial output every 5 seconds shows `Mem=X/70` with warnings:
- `>55 blocks`: MEM HIGH warning
- `>=65 blocks`: MEM CRITICAL warning (audio will fail)

**NEVER**:
- Call `recQueue.begin()` in `setup()` (wastes 53 blocks when idle)
- Access the queue when not recording (check `wav.isRecording()` first)
- Reduce `AudioMemory()` below 70 (need headroom for queue + synth)

**Technical details**:
- Each block = 256 bytes (128 samples × 2 bytes/sample)
- Blocks must be in fast DTCM RAM (not PSRAM - too slow for audio interrupt)
- `AudioRecordQueue` default buffer = 53 blocks = 13.6KB
- Queue drains to SD via `wav.update()` (called 3x per loop when recording)

## Important Implementation Details

### Effects System

Three toggleable effects modes (encoder button cycles through):
- **OFF**: Clean, all lo-fi effects disabled
- **BITCRUSH**: 6-bit quantization
- **DISTORT**: XOR distortion + wavefolding
- **DELAY**: Sample rate reduction for lo-fi texture

Effects are applied by setting parameters on the `AudioSynthProjectCat` voice, not separate audio objects.

### Pot Input Smoothing

Raw ADC readings are noisy. All pot inputs use **exponential smoothing**:
```cpp
smoothValue += POT_ALPHA * (rawValue - smoothValue);
```
- `POT_ALPHA = 0.05f` balances responsiveness vs. noise rejection
- Applied to all four pots: cutoff (A0), drive (A1), decay (A2), pitch (A3)

### Sound Presets

32 built-in sounds (selected via D9 button):
- **Waveforms** (0-5): SAW, SINE, SQUARE, TRI, NOISE, KARPLUS
- **Drums** (6-15): KICK, SNARE, HIHAT, CLAP, TOM, COWBELL, RIMSHOT, CYMBAL, WOODBLOCK, SHAKER
- **Synths** (16-27): BASS, LEAD, PAD, BELL, PLUCK, ORGAN, BRASS, STRINGS, ARP, CHOIR, RHODES, WOBBLE
- **Special FX** (28-31): LASER, BLEEP, ZAP, GLITCH

Presets configure multiple parameters (waveform, cutoff, resonance, drive, attack, decay, envelope). See `applySoundPreset()` in the .ino file for parameter details.

**Important**: Pot values override preset parameters in real-time. This is intentional — pots always have priority.

**Recommended combinations**:
- WOBBLE preset + LFO effect = classic dubstep bass
- LASER/ZAP/GLITCH presets + BITCRUSH/DISTORT effects = retro game sounds
- PLUCK preset + high decay pot = sustained string pad
- Any drum + DELAY effect = lo-fi drum machine

### Recording System (FULLY ENABLED - Fixed 2026-06-24)

WAV recording is now fully functional with lazy queue initialization to prevent audio dropouts.

**How it works**:
- Press D9 button to start/stop recording
- Files saved as `/sample000.wav`, `/sample001.wav`, etc. (auto-incremented)
- 16-bit 44.1kHz mono WAV format (CD quality)
- Uses `AudioRecordQueue` → `WavWriter` → SD card pipeline
- Queue only runs when recording (saves 53 audio blocks when idle)

**SD card requirements**:
- Format: FAT32 (<32GB) or exFAT (32GB-128GB)
- Insert in Teensy 4.1 bottom SDIO slot
- Must be formatted before first use

**Technical details**:
- `recQueue.begin()` called in `startRecording()` (allocates ~53 blocks)
- `recQueue.end()` called in `stopRecording()` (frees blocks)
- `wav.update()` drains queue 3x per loop when recording
- Queue draining prevents buffer overflow and audio dropouts

### Display Pages

Six OLED pages (rotary encoder cycles):
1. **LIVE**: Real-time status, pot values, current sound/effect
2. **TONE**: Cutoff, drive, resonance bars
3. **ENV**: Envelope settings
4. **LOFI**: Lo-fi effects info
5. **REC**: Recording status (disabled)
6. **SCOPE**: Live waveform visualization (1ms update rate)

The SCOPE page generates a simulated waveform based on the current sound preset and voice activity. It's not a true audio buffer visualization (too expensive for OLED refresh rate).

## Hardware Wiring Reference

Critical connections (see WIRING.md for full details):

**I2S Audio** (PCM5102):
- BCLK → Pin 21
- LRCLK → Pin 20
- DIN → Pin 7
- SCK → GND (slave mode)
- XSMT → 3.3V (unmute)

**I2C Display** (SSD1306):
- SDA → Pin 18
- SCL → Pin 19
- Address: 0x3C

**Control Inputs**:
- A0-A3: Potentiometers (10kΩ linear, wiper to analog pin, 3.3V/GND on ends)
- D8: Trigger button (INPUT_PULLUP, active LOW) - plays note
- D9: Recording toggle button (INPUT_PULLUP, active LOW) - start/stop WAV recording
- Pins 2/3/4: Rotary encoder (A/B/SW, INPUT_PULLUP) - page scroll + effects/sound selection
  - Rotation: cycle OLED pages
  - Short press (<1s): cycle effects (OFF/BITCRUSH/DISTORT/DELAY/FM/LFO)
  - Long press (≥1s): cycle sounds (16 presets)

**SD Card**: Built-in SDIO slot (bottom of Teensy 4.1), no external wiring

**Available pins for expansion**:
- Digital: 5, 6, 10, 11, 12, 13, 14, 15, 16, 17, 22, 23, 24-33, 38, 39
- Analog: A4-A9, A10-A13 (also usable as digital)
- I2S occupied: 7 (DIN), 20 (LRCLK), 21 (BCLK)
- I2C occupied: 18 (SDA), 19 (SCL)

## Debugging Tips

**Serial output** shows three periodic reports:
1. **1 Hz pot debug**: Raw ADC values and smoothed parameters
2. **5 Hz audio health**: CPU/memory usage with warnings if >80% or >50 blocks
3. **Boot messages**: 7-step startup sequence (pin setup → audio → voice → I2C → OLED → SD → done)

**Common issues**:
- **No sound**: Check I2S wiring, XSMT pin (must be HIGH), Serial shows "Note: ON"
- **Pots not responding**: Serial shows constant ADC values if unwired (expected)
- **OLED blank**: Try address 0x3D, check SDA/SCL, I2C pullups
- **Audio dropouts**: Check CPU usage report, increase `AudioMemory()` allocation

**Encoder glitches**: The encoder uses a detent-filtered quadrature decoder (`pollEncoderStep()`) to prevent phantom rotations. It only commits a step when returning to rest position (A=1, B=1) after ±2 sub-steps.

## Code Style and Conventions

- **Fixed-point avoided**: All DSP uses 32-bit float (Cortex-M7 FPU is faster than integer math)
- **No dynamic allocation** in audio path (pre-allocated AudioMemory blocks only)
- **Avoid Serial.print in loop()**: Use timed `elapsedMillis` to rate-limit debug output
- **Parameter ranges**: Always constrain inputs (see setter methods in AudioSynthProjectCat.h)
- **Comments**: DSP blocks are heavily commented with the chain order and algorithm details

## Project History Notes

This is a **port from an ESP32-S3 version** of Project Cat. Key differences:
- ESP32 used FreeRTOS dual-core (core 0 for audio, core 1 for UI)
- Teensy uses cooperative multitasking (audio in DMA interrupt, UI in `loop()`)
- FM and LFO were enabled by default on ESP32, disabled on Teensy for cleaner sound
- Recording system was stable on ESP32, needs debugging on Teensy

When modifying the DSP chain, refer to the ESP32 version's behavior as the reference implementation.

## Future Expansion Plans

### Planned Features

1. **WAV Recording Re-enablement**: Currently disabled due to SD init issues and audio dropouts. Code is commented out but preserved.

2. **Encoder Menu System**: Add deep-dive parameter editing for effects amounts (bitcrush depth, XOR intensity, wavefold amount, sample rate reduction).

3. **FM/LFO Re-enablement**: Code exists but disabled (caused drone on ESP32 port). Needs testing.

### Hardware Expansion: CD4017 Analog Sequencer

**Goal**: "LMMS on the go" using analog hardware for organic sound and tactile control.

**Concept**: Add CD4017 decade counter chips to create hardware step sequencers. Each chip outputs 10 sequential steps that can control synth parameters via resistor ladder networks feeding analog inputs.

**Key approaches documented in PROJECT_NOTES.md**:
- Single CV sequencer (pitch only)
- Multi-parameter sequencer (cutoff, drive, decay, pitch)
- Digital preset sequencer (step through 16 sound presets)

**Available resources**: Teensy has plenty of free digital pins (5, 6, 10-13, 22-23+) and could output PWM clock on D10 to drive external sequencer chips.

### Modular Analog Effects System

**Goal**: Pluggable analog effect modules that can be enabled/disabled independently - "stomp box inside the synth" approach.

**Architecture**: Insert effects between PCM5102 DAC output and LM386 amplifier input. Each module has bypass switching (DPDT, relays, or insert jacks).

**Documented modules** (11 types in PROJECT_NOTES.md):
1. **Audio Effects**: PT2399 delay, spring reverb, distortion/fuzz, VCF, chorus
2. **I/O Expansion**: MIDI in, external audio input, CV/Gate jacks (in and out)
3. **Sequencer Enhancement**: Clock divider/multiplier, analog mixer

**Implementation strategies**:
- Stomp box style (individual enclosures, most flexible)
- Eurorack-style modules (rack-mounted, normalized connections)
- Integrated switchboard (all on one PCB with switches)
- Insert jacks (TRS send/return loops)

**Recommended build order**: Fix pots → CD4017 sequencer → MIDI input → PT2399 delay → bypass switching

See PROJECT_NOTES.md "Modular Analog Effects Chain" section for complete module library, circuits, parts list, and wiring strategies.
