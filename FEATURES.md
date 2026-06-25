# Project Cat - Feature Documentation

**Last Updated**: 2026-06-24
**Platform**: Teensy 4.1
**Firmware Version**: 1.0 (Stable)

---

## 🎵 Core Features

### Audio Synthesis Engine
- **Custom DSP Chain**: 11-stage audio processing pipeline
- **Sample Rate**: 44.1kHz (CD quality)
- **Bit Depth**: 16-bit signed integer
- **Latency**: <3ms (128-sample blocks)
- **CPU Usage**: 10-20% typical (single voice)

### Sound Library
**32 Built-in Presets** across 4 categories:

#### 1. Basic Waveforms (0-5)
- **SAW** - Classic sawtooth wave (bright, buzzy)
- **SINE** - Pure sine wave (smooth, clean)
- **SQUARE** - Square wave (hollow, woody)
- **TRI** - Triangle wave (soft, mellow)
- **NOISE** - White noise generator
- **KARPLUS** - Karplus-Strong string synthesis (plucked string)

#### 2. Drum Sounds (6-15)
- **KICK** - Deep bass drum (sine wave, short decay)
- **SNARE** - Snappy snare drum (noise + resonance)
- **HIHAT** - Crisp hi-hat (bright noise, very short)
- **CLAP** - Hand clap (mid-range noise burst)
- **TOM** - Tom drum (sine with moderate decay)
- **COWBELL** - Classic cowbell (square wave, high resonance)
- **RIMSHOT** - Sharp rim hit (square wave, metallic)
- **CYMBAL** - Bright crash cymbal (full-spectrum noise)
- **WOODBLOCK** - Hollow wooden tone (triangle wave)
- **SHAKER** - High rattling percussion (filtered noise)

#### 3. Synthesizer Patches (16-27)
- **BASS** - Deep, warm bass (saw wave, low cutoff)
- **LEAD** - Bright, cutting lead (saw wave, high cutoff)
- **PAD** - Soft, smooth pad (triangle wave, long decay)
- **BELL** - Bell-like tone (sine wave, high resonance)
- **PLUCK** - Pizzicato string pluck (Karplus-Strong)
- **ORGAN** - Full-bodied organ (sine wave, sustained)
- **BRASS** - Bright, punchy brass (saw wave, driven)
- **STRINGS** - Smooth string pad (saw wave, slow attack)
- **ARP** - Sharp arpeggio sound (square wave, staccato)
- **CHOIR** - Soft vocal pad (triangle wave, slow attack)
- **RHODES** - Electric piano tone (sine wave, moderate decay)
- **WOBBLE** - Dubstep wobble bass (saw wave, high resonance)

#### 4. Special FX (28-31)
- **LASER** - Sci-fi laser zap (saw wave, pitch sweep)
- **BLEEP** - Digital beep/bloop (square wave, short)
- **ZAP** - Electric discharge (noise, very aggressive)
- **GLITCH** - Broken digital artifact (square wave, ring mod)

---

## 🎛️ Real-Time Controls

### Hardware Inputs

#### Potentiometers (A0-A3)
All pots use 12-bit ADC with exponential smoothing (POT_ALPHA = 0.05):

1. **A0 - CUTOFF** (0.0-1.0)
   - Controls lowpass filter frequency
   - Range: 20Hz to ~18kHz (logarithmic)
   - Overrides preset cutoff in real-time

2. **A1 - DRIVE** (0.0-1.0)
   - Controls tanh waveshaper saturation
   - Range: 1× to 16× gain
   - Adds analog-style warmth and grit

3. **A2 - DECAY** (20ms to 2 seconds)
   - Controls envelope decay time
   - Exponential mapping for musical feel
   - Works in both AD and ASR envelope modes

4. **A3 - PITCH** (55Hz to 880Hz)
   - Controls oscillator frequency
   - Range: 4 octaves (A1 to A5)
   - Real-time pitch control while note is held

#### Buttons

1. **D8 - TRIGGER**
   - Press and hold to play note
   - Release to stop note
   - Uses pitch value from A3 pot
   - 20ms debouncing

2. **D9 - SOUND SELECT**
   - Cycles through 32 presets
   - Wraps around (31 → 0)
   - Applies preset parameters immediately
   - 20ms debouncing

3. **D10 - RECORD** (NEW)
   - Toggles WAV recording on/off
   - Auto-increments filename (/sample000.wav, /sample001.wav, etc.)
   - LED indicator on OLED display
   - 20ms debouncing

#### Rotary Encoder (Pins 2/3/4)

1. **Rotation** - Cycle OLED display pages
   - Detent-filtered quadrature decoding
   - 6 pages: LIVE → TONE → ENV → LOFI → REC → SCOPE
   - Wraps around in both directions

2. **Button Press** - Cycle effects
   - 6 effects modes: OFF → BITCRUSH → DISTORT → DELAY → FM → LFO
   - Wraps around (LFO → OFF)
   - No debouncing needed (press event only)

---

## 🎚️ Effects System

### Available Effects (Encoder Button)

1. **OFF** - Clean, bypassed
   - All effects disabled
   - Pure synth voice signal
   - Lowest CPU usage

2. **BITCRUSH** - Lo-fi degradation
   - Reduces bit depth to 6-bit
   - Creates digital grit and noise floor
   - Great for retro game sounds

3. **DISTORT** - Digital distortion
   - XOR distortion (amount = 0.4)
   - Wavefolder (amount = 0.3, 6 iterations)
   - Harsh, metallic character

4. **DELAY** - Lo-fi texture
   - Sample rate reduction (50%)
   - Creates decimation artifacts
   - Aliasing for lo-fi character

5. **FM** - Frequency modulation
   - FM ratio: 2.0 (harmonic)
   - FM depth: 0.3 (moderate)
   - Metallic, bell-like tones

6. **LFO** - Vibrato/tremolo
   - LFO rate: 5Hz
   - LFO depth: 0.05 (subtle)
   - Pitch modulation for vibrato

**Note**: Effects apply to all sounds. Combine with presets for hybrid tones.

---

## 📊 OLED Display (128x64)

### 6 Display Pages (Rotate with Encoder)

1. **LIVE** - Main status page
   - Current sound name
   - Current effect name
   - Trigger status (NOTE ON/OFF)
   - Recording status (REC indicator)
   - All 4 pot values (Cutoff/Drive/Decay/Pitch)

2. **TONE** - Filter parameters
   - Cutoff bar graph (0-100%)
   - Drive bar graph (0-100%)
   - Resonance value (read-only, preset-dependent)
   - Visual representation of tone shaping

3. **ENV** - Envelope settings
   - Attack time (milliseconds)
   - Decay time (seconds)
   - Envelope stage (IDLE/ATTACK/DECAY/SUSTAIN/RELEASE)
   - Envelope mode (AD or ASR)

4. **LOFI** - Lo-fi effect details
   - Bit depth (1-16 bits)
   - Sample rate reduction (0-100%)
   - XOR distortion amount (0-100%)
   - Wavefold amount (0-100%)

5. **REC** - Recording status
   - Recording indicator (ON/OFF)
   - Elapsed recording time (seconds)
   - SD card free space (MB)
   - Current filename

6. **SCOPE** - Waveform visualization
   - Real-time simulated waveform display
   - Shows current preset's base waveform
   - Updates at 10ms intervals
   - Visual feedback for sound selection

**Update rate**: 10 FPS (100ms refresh) to prevent I2C blocking

---

## 💾 WAV Recording System

### Features
- **Format**: 16-bit PCM, 44.1kHz, mono
- **Storage**: MicroSD card (SDIO interface)
- **File naming**: Auto-incrementing `/sample000.wav` through `/sample999.wav`
- **Quality**: CD-quality audio, same as live output
- **Lazy initialization**: Queue only runs during recording (saves 53 audio blocks when idle)

### Supported SD Cards
- **FAT32**: Up to 32GB (recommended for best compatibility)
- **exFAT**: 32GB to 128GB (requires Teensy 4.1 support)
- **Format**: Must be formatted before first use

### Recording Workflow
1. Insert SD card in Teensy 4.1 bottom slot
2. Power on (watch Serial Monitor for "SD OK!" message)
3. Press **D10** to start recording
4. OLED shows "REC" indicator and elapsed time
5. Play sounds with D8 trigger button
6. Press **D10** again to stop recording
7. WAV file is finalized and saved
8. Next recording auto-increments filename

### Technical Details
- **AudioRecordQueue**: 53 blocks (~13.6KB) allocated only during recording
- **Drain rate**: 3× `wav.update()` calls per loop iteration
- **Buffer management**: Automatic queue draining prevents overflow
- **Memory efficiency**: Queue freed immediately when recording stops
- **File format**: Standard RIFF/WAVE header with proper size fields

---

## 🔊 DSP Signal Chain

**Processing order** (fixed, cannot be reordered):

1. **Source Oscillator**
   - 6 waveforms: SAW, SINE, SQUARE, TRI, NOISE, KARPLUS-STRONG
   - Phase accumulator with wraparound
   - Karplus-Strong uses 2048-sample delay line

2. **FM Modulation** (optional)
   - Phase modulation synthesis
   - Modulator frequency = carrier × FM ratio
   - Adds harmonic/inharmonic partials

3. **LFO Modulation** (optional)
   - Sine wave LFO (5Hz default)
   - Pitch modulation (vibrato)
   - Depth control (0.05 default)

4. **Sample Rate Reducer**
   - Sample-and-hold decimation
   - Reduction factor: 1× to 61×
   - Creates aliasing artifacts

5. **Bitcrusher**
   - Amplitude quantization
   - 1-bit to 16-bit depth
   - Reduces dynamic range

6. **XOR Distortion**
   - Bitwise XOR with mask
   - Mask = amount × 4095
   - Digital, harsh character

7. **Wavefolder**
   - Reflective wavefolding
   - 6-iteration clamp loop
   - Gain = 1 + (amount × 4)

8. **Tanh Waveshaper**
   - Hyperbolic tangent saturation
   - Gain = 1 + (drive × 15)
   - Analog-style soft clipping

9. **Resonator**
   - Pitch-tracking 2-pole IIR filter
   - Coefficients update per block
   - Stable r = 0.80 to 0.98
   - Adds harmonic ringing

10. **TPT State-Variable Filter**
    - Cytomic topology (trapezoidal integration)
    - Lowpass output tap
    - Cutoff: 20Hz to ~18kHz (log scale)
    - Resonance: 0 to 0.98 (self-oscillation at max)

11. **AD/ASR Envelope**
    - Attack: linear ramp (0.5ms to 2s)
    - Decay: exponential fall (5ms to 8s)
    - Sustain: holds at peak (if sustainMode = true)
    - Release: same as decay curve
    - Mode: AD (default) or ASR (optional)

12. **Output Gain**
    - Final level control
    - Default: 0.8 (80%)
    - Applied after envelope

**Block processing**: Coefficients computed once per 128-sample block, samples rendered individually.

---

## 🧠 Memory Management

### Audio Memory Allocation
```cpp
AudioMemory(70);  // Total pool: 70 blocks × 256 bytes = 17.9KB
```

**Memory usage breakdown**:
- **Synth voice**: ~10-15 blocks (idle)
- **AudioRecordQueue**: ~53 blocks (when recording)
- **I2S output**: ~2 blocks
- **Headroom**: ~5-10 blocks

### Critical Memory Rules
1. **Never call `recQueue.begin()` at boot** - wastes 53 blocks
2. **Always call `recQueue.end()` when stopping** - frees blocks immediately
3. **Monitor memory usage** via Serial (reports every 5 seconds)
4. **Warnings**:
   - `>55 blocks`: MEM HIGH
   - `>=65 blocks`: MEM CRITICAL (audio will fail)

### Why This Matters
- **Fast DTCM RAM required**: PSRAM too slow for audio interrupts
- **Shared pool**: All audio objects draw from same AudioMemory allocation
- **Dropouts occur** when pool exhausted (allocate() returns NULL)
- **Lazy queue init** solves the problem: 53 blocks only used when recording

---

## 🐛 Debugging & Monitoring

### Serial Output (115200 baud)

#### Boot Sequence (7 steps)
1. Pin setup
2. Audio memory allocation
3. Voice configuration
4. I2C initialization
5. OLED setup
6. SD card initialization
7. Final ready message

#### Periodic Reports

**Pot Diagnosis** (every 5 seconds):
```
===== POT DIAGNOSIS =====
RAW ADC: A0=2048 A1=1024 A2=3072 A3=512
NORMALIZED (0-1): Cut=0.500 Drv=0.250 Dec=0.750 Pitch=0.125
SMOOTHED: Cut=0.498 Drv=0.252 Dec=0.748 Pitch=0.127
APPLIED: Decay=1.516s Pitch=98Hz
SOUND: BASS | FX: DISTORT
=========================
```

**Audio Health** (every 5 seconds):
```
AUDIO: CPU=12.3% (max:15.8%) Mem=14/70 [PLAYING]
AUDIO: CPU=8.1% (max:10.2%) Mem=62/70 [IDLE]
```

**Warnings**:
- `*** CPU OVERLOAD! ***` - CPU >80%
- `*** CPU SPIKE DETECTED! ***` - Max CPU >95%
- `*** MEM HIGH! ***` - Memory >55 blocks
- `*** MEM CRITICAL - AUDIO WILL FAIL! ***` - Memory ≥65 blocks
- `*** SLOW LOOP: XXXXX us ***` - loop() took >10ms

### Common Issues & Solutions

| Issue | Symptom | Solution |
|-------|---------|----------|
| **No sound** | Silence on output | Check I2S wiring, XSMT=HIGH, Serial "NOTE ON" |
| **Distortion on rapid triggers** | Sound cuts out after few presses | Fixed in v1.0 (lazy queue init) |
| **Pots not responding** | Same ADC values | Check wiring, should be 0-4095 range |
| **OLED blank** | Display not working | Try address 0x3D, check SDA/SCL pullups |
| **SD init failed** | Recording won't start | Format card FAT32, check bottom slot insertion |
| **Audio dropouts** | Crackling/stuttering | Check CPU/mem reports, reduce effects |
| **Encoder glitches** | Phantom rotations | Fixed (detent-filtered quadrature) |

---

## 🎹 Performance Specifications

### Timing Characteristics
- **Audio block time**: 2.9ms (128 samples @ 44.1kHz)
- **DMA interrupt rate**: 344Hz (every 2.9ms)
- **Loop() execution**: <1ms typical, <10ms max
- **UI refresh**: 100ms (10 FPS)
- **Serial reports**: 5000ms (0.2 Hz)
- **Scope update**: 10ms (100 FPS)

### CPU Usage by Component
- **AudioSynthProjectCat::update()**: ~8-15% (varies by waveform/effects)
- **I2S DMA transfer**: <2%
- **RecordQueue** (when active): ~3-5%
- **Total audio system**: ~10-22% (plenty of headroom)

### Latency Analysis
1. **Input to audio**: <1ms (digitalRead + immediate noteOn)
2. **Audio processing**: 2.9ms (one block)
3. **I2S output buffering**: ~3ms (double-buffered DMA)
4. **Total system latency**: ~7ms (practically imperceptible)

---

## 🔧 Customization & Hacking

### Adding New Sounds
1. Edit `waveformNames[]` array (ProjectCat_Teensy.ino:99)
2. Increment `NUM_WAVEFORMS` constant
3. Add case to `applySoundPreset()` switch statement
4. Set waveform, cutoff, resonance, drive, attack, decay
5. Update scope visualization mappings (line 760+)
6. Recompile and upload

### Modifying Effects
All effects controlled in encoder button handler (line 668+):
- Adjust bit depth: `voice.setBitDepth(1.0f to 16.0f)`
- Adjust XOR amount: `voice.setXorAmount(0.0f to 1.0f)`
- Adjust wavefold: `voice.setFold(0.0f to 1.0f)`
- Adjust sample rate: `voice.setSampleRateReduce(0.0f to 1.0f)`
- Adjust FM: `voice.setFMRatio()`, `voice.setFMDepth()`
- Adjust LFO: `voice.setLFORate()`, `voice.setLFODepth()`

### Parameter Ranges
All setters constrain input (see AudioSynthProjectCat.h:52-74):
```cpp
setPitch(8.0f to 8000.0f Hz)
setCutoff(0.0 to 1.0 normalized)
setResonance(0.0 to 0.98)
setDrive(0.0 to 1.0)
setAttack(0.5ms to 2s)
setDecay(5ms to 8s)
setBitDepth(1.0 to 16.0 bits)
setXorAmount(0.0 to 1.0)
setFold(0.0 to 1.0)
setSampleRateReduce(0.0 to 1.0)
```

### Thread Safety
**Safe** (single parameter updates from loop):
```cpp
voice.setCutoff(0.5f);
voice.setDrive(0.3f);
```

**Requires protection** (multi-parameter atomic updates):
```cpp
AudioNoInterrupts();
voice.setPitch(440.0f);
voice.noteOn();
AudioInterrupts();
```

**Never do** (inside audio interrupt):
- Call Serial.print()
- Access SD card
- Use delays or blocking code
- Allocate memory dynamically

---

## 🚀 Future Expansion Ideas

### Planned Software Features
- Encoder menu system for deep parameter editing
- Preset save/load to SD card
- MIDI input support (hardware mod required)
- Polyphonic mode (2-4 voices)
- Sequencer/arpeggiator

### Hardware Expansion Possibilities
- **CD4017 step sequencer** (analog CV control)
- **PT2399 delay pedal** (analog insert effect)
- **Spring reverb tank** (insert between DAC and amp)
- **MIDI DIN input** (pin 0 RX, optocoupler circuit)
- **CV/Gate outputs** (DAC IC for external synths)
- **Eurorack case integration** (±12V power, 3U panel)

See CLAUDE.md "Future Expansion Plans" and PROJECT_NOTES.md for detailed circuits and schematics.

---

## 📋 Quick Reference Card

### Button Layout
```
D8  = TRIGGER (play note)
D9  = SOUND SELECT (cycle 32 presets)
D10 = RECORD (toggle WAV recording)
ENC = EFFECTS (cycle 6 effects)
```

### Pot Functions
```
A0 = CUTOFF (filter frequency)
A1 = DRIVE (saturation amount)
A2 = DECAY (envelope time)
A3 = PITCH (note frequency)
```

### Display Pages
```
1. LIVE   - Main status
2. TONE   - Filter controls
3. ENV    - Envelope settings
4. LOFI   - Effect amounts
5. REC    - Recording status
6. SCOPE  - Waveform view
```

### Sound Categories
```
0-5    = Waveforms
6-15   = Drums
16-27  = Synths
28-31  = FX
```

### Effects Chain
```
OFF → BITCRUSH → DISTORT → DELAY → FM → LFO → (wraps)
```

---

## 📖 Additional Documentation

- **CLAUDE.md** - Developer guide, code architecture, build instructions
- **WIRING.md** - Hardware connections, pinout reference (if exists)
- **PROJECT_NOTES.md** - Expansion ideas, circuit schematics (if exists)
- **README.md** - Project overview, quick start (if exists)

---

## 🏆 Changelog

### v1.0 (2026-06-24) - Stable Release
- ✅ Fixed audio memory exhaustion bug (lazy queue initialization)
- ✅ Added D10 recording button (freed D9 for sound selection)
- ✅ Expanded sound library from 16 to 32 presets
- ✅ Added 4 new drum sounds (RIMSHOT, CYMBAL, WOODBLOCK, SHAKER)
- ✅ Added 8 new synth patches (PLUCK, ORGAN, BRASS, STRINGS, ARP, CHOIR, RHODES, WOBBLE)
- ✅ Added 4 special FX sounds (LASER, BLEEP, ZAP, GLITCH)
- ✅ Improved encoder button response (removed long-press, instant toggle)
- ✅ Comprehensive technical documentation (CLAUDE.md, FEATURES.md)
- ✅ Memory management optimization (70 blocks, efficient allocation)

### Known Issues
- None (stable build)

---

**End of Feature Documentation**
