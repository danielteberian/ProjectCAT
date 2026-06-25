# Project Cat - Development Notes

## Project Overview
- **Platform**: Teensy 4.1
- **Audio**: PCM5102 I2S DAC → LM386 → 8Ω speaker
- **Display**: SSD1306 128x64 OLED (I2C)
- **Controls**:
  - 4 pots: A0 (cutoff), A1 (drive), A2 (decay), A3 (pitch)
  - D8: Trigger button (note on/off)
  - D9: Save/Export button (WAV recording to SD)
  - EC11 rotary encoder (page select)

## Audio Signal Chain
1. Source (OSC/Karplus-Strong) + FM + LFO
2. Sample-rate reducer
3. Bitcrusher
4. XOR distortion
5. Wavefolder
6. Tanh waveshaper (drive)
7. Resonator (bounded 2-pole)
8. TPT state-variable lowpass
9. AD/ASR envelope
10. Output gain

## EMERGENCY RECOVERY

**If Teensy won't boot or isn't recognized:**

1. **Enter bootloader mode:**
   - Press and HOLD the white button on Teensy for 15 seconds
   - OR: Unplug → Hold white button → Plug in USB → Release after 2 seconds

2. **Upload minimal version:**
   - Open `MINIMAL_BOOT.ino` in Arduino IDE
   - Upload to Teensy (this will ALWAYS work)
   - Once booted, open Serial Monitor to confirm

3. **Then try main version:**
   - The improved `ProjectCat_Teensy.ino` now has step-by-step boot messages
   - Upload it and watch Serial Monitor to see where it stops (if it does)

**Boot Messages:**
The new version shows 7 steps during boot:
1. Setting up pins
2. Starting audio
3. Setting up voice
4. Starting I2C
5. Initializing OLED (can fail - continues anyway)
6. Initializing SD card (can fail - continues anyway)
7. Final setup

If it stops at a step, that's the problematic hardware.

## Status

### Completed
- [x] Sound output working (continuous tone confirmed)
- [x] Custom AudioSynthProjectCat synth engine implemented
- [x] Audio routing: voice → i2sOut + recQueue
- [x] Pot reading for cutoff, drive, decay, pitch
- [x] WAV recording to SD card
- [x] OLED display integration
- [x] Button-triggered notes (D8 working)
- [x] Pot noise filtering (exponential smoothing)
- [x] SD card formatting function

### Current Configuration (Updated)
- **Pots**: ENABLED - read from A0-A3
  - A0: Cutoff (0-1, controls filter brightness)
  - A1: Drive (0-1, controls saturation/warmth)
  - A2: Decay (0-1, maps to 20ms-2s envelope)
  - A3: Pitch (0-1, maps to 55Hz-880Hz, 4 octaves)
  - If not wired, will show noisy values (ignore)
- **Audio**: Clean (all lo-fi effects disabled)
- **Recording**: COMPLETELY COMMENTED OUT
  - All recording code preserved but disabled with /* */
  - To re-enable: uncomment sections marked with recording comments
  - Reason: Was causing audio queue backup and dropouts

### Working Features
- ✅ D8 button triggers/releases sounds
- ✅ D9 button cycles through 16 sounds:
  - **Waveforms**: SAW, SINE, SQUARE, TRI, NOISE, KARPLUS
  - **Drums**: KICK, SNARE, HIHAT, CLAP, TOM, COWBELL
  - **Synths**: BASS, LEAD, PAD, BELL
- ✅ **Pots control synth in real-time** (A0-A3) - FIXED!
  - **A0 (Cutoff)**: Filter brightness (updates continuously while playing)
  - **A1 (Drive)**: Saturation/warmth
  - **A2 (Decay)**: Envelope decay time
  - **A3 (Pitch)**: Base note frequency (updates while held)
- ✅ **Toggleable Effects** (encoder button):
  - OFF: Clean, no effects
  - BITCRUSH: 6-bit degradation
  - DISTORT: XOR distortion + wavefolding
  - DELAY: Sample rate reduction (lo-fi texture)
- ✅ OLED display with 6 pages (encoder rotation to cycle):
  - LIVE: Status, parameters, **current effect shown**
  - TONE: Cutoff, Drive, Resonance bars
  - ENV: Envelope settings
  - LOFI: Lo-fi effects info
  - REC: Recording status
  - **SCOPE**: Live waveform visualization - IMPROVED!
    - Faster update rate (1ms vs 2ms)
    - Shows actual waveform shape for each sound
    - Resets phase when switching sounds (no glitching)
    - Tighter display period (15x faster advancement)
- ✅ Both L+R audio channels
- ✅ Stable audio (no dropouts)
- ✅ Serial debug shows pot values every second
- ✅ Complete wiring guide in WIRING.md

### How to Re-enable Recording Later
1. Uncomment recording system in audio graph (line ~48)
2. Uncomment recording functions (startRecording, stopRecording, etc.)
3. Uncomment SD init code in setup()
4. Uncomment wav.update() calls in loop()
5. Change D9 button from waveform cycling back to recording
6. Test that queue drains fast enough

### Troubleshooting WAV Recording (D9 button)
Check Serial Monitor for these messages when pressing D9:
1. **Button detection**: Should see "D9 button pressed!"
   - If NOT seen → Check D9 wiring / button connection
   - Watch debug output: "D9:1" (not pressed) → "D9:0" (pressed)
2. **File creation**: Should see "Attempting to create file: /sample###.wav"
   - If file creation fails → SD card issue (not formatted or not inserted)
3. **Recording status**: Should see "SUCCESS! Recording to..."
   - If FAILED → Format SD card as FAT32 on computer

### Recent Fixes
- [x] **CRITICAL: Audio dropout fixed!** - AudioRecordQueue was filling up and blocking audio
  - Queue now drains continuously whether recording or not
  - Increased AudioMemory from 40 to 60 blocks
  - Added right channel output
  - Audio should be stable now!
- [x] Button triggering sound - WORKING!
- [x] Pot noise/fluctuation - Disabled pots (not wired), using fixed values
  - Fixed values prevent floating input noise
- [x] SD card detection - Added 3-attempt retry on init
  - Better error messages for troubleshooting
- [x] Button debouncing - Added 20ms debounce to prevent missed/double triggers

### Next Steps
1. Test WAV recording with D9 button - check Serial output for:
   - "SD card initialized OK" message
   - "Starting recording..." / "Stopping recording..." messages
   - File path and bytes written
2. If SD fails, check:
   - SD card is inserted
   - Card is formatted (FAT32 recommended)
   - SDIO wiring if using external SD module

## Configuration Notes
- Envelope mode: AD (not ASR) - sustainMode = false
- FM: OFF (was ON in ESP32 version)
- LFO: OFF (was ON in ESP32 version)
- Default waveform: SAWTOOTH
- Attack: 5ms, Decay: 400ms (adjustable via pot)
- Output gain: 0.8

## SD Card Formatting & Access

### Option 1: USB Mass Storage Mode (Recommended for 128GB cards)

A separate project `Teensy_SD_MassStorage` lets you access the SD card like a USB flash drive on your Mac:

1. Open `Teensy_SD_MassStorage/Teensy_SD_MassStorage.ino`
2. **Tools → USB Type → "Serial + MTP Disk (Experimental)"**
3. Upload to Teensy
4. Mac will show SD card as removable drive
5. Format as **exFAT** (for 128GB card)
6. When done, upload ProjectCat sketch back (set USB Type to "Serial")

**See `Teensy_SD_MassStorage/README.md` for full instructions**

### Option 2: Format via Teensy (Small cards only)

For cards <32GB, you can format directly:

1. **Make sure device is booted** (wait for "BOOT COMPLETE" in Serial Monitor)
2. **Hold BOTH buttons** (D8 + D9) simultaneously for **3 seconds**
3. Serial will show: `>>> STARTING SD CARD FORMAT <<<`
4. **Wait** - can take 10-30 seconds
5. Watch Serial Monitor for status

**Note:** 128GB cards will likely fail - use Option 1 instead

### Option 3: Remove card and format on Mac

1. Power off Teensy
2. Remove microSD from bottom slot
3. Insert into Mac (card reader/adapter)
4. Disk Utility → Erase → exFAT
5. Reinsert into Teensy

**⚠️ WARNING:** Formatting erases ALL files on the SD card!

## Debug Features
- Serial debug @ 200ms: shows all pot/button values
- CPU usage report @ 1s: shows audio engine load
- Display refresh @ 20fps: shows params + recording status

## Future Hardware Expansion Ideas

### CD4017 Analog Sequencer
**Concept**: "LMMS on the go" but using analog hardware for nice sound and neat design

**Overview**: Use CD4017 decade counter chips to create hardware step sequencers that control synth parameters via analog voltages. Keeps the organic, tactile feel of analog while adding sequencing capability.

**Implementation Options**:

#### Option 1: Pitch CV Sequencer (Simple)
- 1x CD4017 + resistor ladder network
- Each of 10 outputs connects through different resistor values
- Output voltage fed to A3 (pitch input)
- Clock source: 555 timer, external clock, or Teensy PWM output
- Result: 10-step pitch sequence (set each step with resistor values)

**Example resistor ladder** (for 0-3.3V steps):
```
CD4017 Q0 ─┬─ 10kΩ ──┐
CD4017 Q1 ─┼─ 22kΩ ──┤
CD4017 Q2 ─┼─ 33kΩ ──┤
...        │         ├──→ to Teensy A3 (pitch)
CD4017 Q9 ─┴─ 100kΩ ─┘
```

#### Option 2: Multi-Parameter Sequencer (Advanced)
- 3-4x CD4017 chips, all clocked together
- CD4017 #1 + ladder → A0 (cutoff sequence)
- CD4017 #2 + ladder → A1 (drive sequence)
- CD4017 #3 + ladder → A3 (pitch sequence)
- CD4017 #4 + ladder → A2 (decay sequence)
- Creates synchronized multi-dimensional sequences

#### Option 3: Preset/Sound Sequencer (Digital)
- Wire CD4017 outputs as binary count to Teensy digital pins
- Read step number (0-9) in software
- Call `applySoundPreset(step)` to sequence through different sounds
- No resistor ladder needed, pure digital

**Hardware Needed**:
- 1-4x CD4017BE decade counter ICs
- 10x resistors per chip (for analog CV mode)
- Clock source: 555 timer, CD4024 divider, or Teensy PWM
- Optional: Reset pin wiring for <10 step sequences
- Optional: Potentiometer on 555 for tempo control

**Available Teensy Pins** (after current build):
- Digital: 5, 6, 10, 11, 12, 13, 22, 23+ (plenty for sequencer I/O)
- Could use D5-D7 for binary step count input
- Could use D10 as clock output to drive CD4017

**Benefits**:
- Tactile, hardware-based sequencing
- No computer needed (standalone groovebox)
- Organic analog CV control (smooth pitch sweeps)
- Low cost (~$1 per CD4017)
- Can be clocked/synced to external gear

**Next Step**: Prototype single CD4017 pitch sequencer on breadboard

---

### Modular Analog Effects Chain
**Concept**: Pluggable/switchable analog effect modules in the audio path (post-DAC, pre-amp)

**Philosophy**: Keep each effect as a separate physical module that can be enabled/disabled via:
- Hardware bypass switches (true bypass with DPDT switches)
- Insert jacks (effect loop send/return)
- Physical module connectors (Eurorack-style or custom)

**Signal Path Architecture**:
```
Teensy → PCM5102 DAC → [Module 1] → [Module 2] → [Module 3] → LM386 Amp → Speaker
                         ↑             ↑             ↑
                      Bypass SW     Bypass SW     Bypass SW
```

---

## Analog Effect Modules Library

### Audio Effects (Post-DAC Processing)

#### 1. PT2399 Analog Delay Module
- **Type**: Warm, lo-fi analog delay/echo
- **Cost**: $5-10 (pre-made module) or $2-3 (chip + passives)
- **Controls**:
  - Delay Time (pot)
  - Feedback/Repeats (pot)
  - Wet/Dry Mix (pot)
- **Power**: 5V (can share Teensy supply)
- **Sound**: Vintage tape-style delays, warm degradation
- **Bypass**: DPDT footswitch or toggle
- **Notes**: Great for rhythmic delays, can self-oscillate

#### 2. Spring Reverb Tank
- **Type**: Mechanical reverb (classic guitar amp sound)
- **Cost**: $15-30 for small tank + driver/recovery circuit
- **Controls**:
  - Reverb amount (pot)
  - Tone (optional)
- **Sound**: Splashy, vintage, responsive to movement/kick
- **Alternative**: Belton Brick digital reverb module ($10, easier)
- **Mounting**: Needs isolation from vibration
- **Bypass**: DPDT switch

#### 3. Analog Distortion/Fuzz Module
- **Type**: Opamp overdrive or transistor fuzz
- **Cost**: <$5 in parts
- **Circuits to try**:
  - RAT-style opamp distortion
  - Simple diode clipper
  - Transistor fuzz (Fuzz Face style)
  - Tube Screamer clone
- **Controls**:
  - Drive/Gain
  - Tone/Filter
  - Output Level
- **Note**: Complements the digital drive control in synth

#### 4. Voltage-Controlled Analog Filter (VCF)
- **Type**: OTA-based filter (LM13700)
- **Cost**: $3-5 for chip + passives
- **CV Control**: Teensy PWM → smoothing filter → CV input
- **Filter Types**: Lowpass, highpass, bandpass
- **Use Cases**:
  - Sequenced filter sweeps (CD4017 controls CV)
  - Envelope-controlled filter (Teensy generates CV)
  - Post-processing filter separate from synth engine
- **Complexity**: Medium (needs careful biasing)

#### 5. Analog Chorus/Flanger
- **Type**: Bucket-brigade delay (MN3007/MN3207)
- **Cost**: $10-20 (BBD chips getting rare)
- **Sound**: Lush modulation, vintage shimmer
- **Easier Alternative**: Use PT2399 with fast LFO modulation

### Input/Output Expansion Modules

#### 6. MIDI Input Module
- **Type**: 5-pin DIN MIDI → Teensy Serial
- **Cost**: ~$2 (6N138 optocoupler + DIN jack + resistors)
- **Function**:
  - Receive MIDI notes → trigger synth
  - MIDI CC → control parameters
  - MIDI clock → sync sequencer
- **Code**: Use Teensy MIDI library (built-in)
- **Benefit**: Play from keyboards, DAWs, other gear

#### 7. External Audio Input Module
- **Type**: Line/instrument preamp → Teensy ADC
- **Cost**: $3-8 (opamp preamp circuit)
- **Function**: Process external audio through synth's digital effects
- **Uses**:
  - Guitar → bitcrusher/filter/drive
  - Vocals → lo-fi effects
  - Drum machine → processing
- **Switching**: Could toggle between internal synth and external input

#### 8. CV/Gate Input Jacks
- **Type**: Standard 3.5mm or 1/4" jack inputs
- **Connections**:
  - Pitch CV → Teensy analog pin (with protection/scaling)
  - Gate In → Teensy digital pin (trigger notes)
  - Filter CV → analog pin
  - Sequencer Clock → digital pin
- **Standard**: 0-5V or 0-3.3V (Teensy-safe with resistor divider)
- **Benefit**: Eurorack/modular integration

#### 9. CV/Gate Output Jacks
- **Type**: Teensy generates CV to control external gear
- **Outputs**:
  - Gate Out (when note is on)
  - Pitch CV (PWM → low-pass filter)
  - Envelope CV (follows ADSR)
  - Clock Out (sequencer tempo)
- **Use**: Control other synths, modular gear, lights

### Sequencer Enhancement Modules

#### 10. Clock Divider/Multiplier (CD4024)
- **Type**: Frequency divider IC
- **Cost**: <$1
- **Function**:
  - Divide main clock by 2, 4, 8, 16, 32, 64
  - Create polyrhythms (different sequencers at different rates)
- **Wiring**: Main clock → CD4024 → multiple outputs
- **Use**: Drive multiple CD4017s at related tempos

#### 11. Analog Mixer Module
- **Type**: Opamp summing mixer (TL072/TL074)
- **Cost**: $2-5
- **Channels**: 3-4 inputs with individual level controls
- **Use Cases**:
  - Mix multiple synth voices
  - Mix synth + external audio
  - Sub-mixes for effect sends
- **Output**: Mixed signal → effects chain

---

## Modular Implementation Strategies

### Strategy 1: Stomp Box Style
- Each effect in its own enclosure
- 1/4" input/output jacks
- Footswitch or toggle for bypass
- Daisy-chain with patch cables
- **Pros**: Most flexible, each box standalone
- **Cons**: Lots of cables, takes up space

### Strategy 2: Eurorack-Style Modules
- Each effect on a small PCB with front-panel controls
- Mount in a rack case with power distribution
- Normalized connections (auto-bypass when module removed)
- **Pros**: Compact, modular, looks professional
- **Cons**: Needs rack/case, more complex mounting

### Strategy 3: Integrated Switchboard
- All effects on one PCB/perfboard
- DPDT switches or relays for each effect
- Single enclosure with all controls
- Optional: Teensy-controlled relay switching (digital enable/disable)
- **Pros**: Compact, no cables, clean
- **Cons**: Less modular, harder to modify

### Strategy 4: Insert Jacks (Studio Style)
- TRS jacks for effect send/return loops
- Normal through-connection when nothing plugged in
- Plug in external effects as needed
- **Pros**: Maximum flexibility, use any effect
- **Cons**: Requires TRS jacks and normalling

---

## Recommended Build Order

### Phase 1: Essential (get these working first)
1. ✅ Fix pot wiring (3 pins per pot!)
2. 🔲 CD4017 pitch sequencer (basic)
3. 🔲 MIDI input (huge capability unlock)

### Phase 2: Sound Enhancement
4. 🔲 PT2399 delay module (biggest bang for buck)
5. 🔲 Simple distortion/fuzz circuit
6. 🔲 Bypass switching system (pick a strategy)

### Phase 3: Advanced
7. 🔲 External audio input
8. 🔲 CV/Gate I/O jacks
9. 🔲 Multi-parameter sequencer (3+ CD4017s)
10. 🔲 Spring reverb or Belton brick

### Phase 4: Ecosystem
11. 🔲 Voltage-controlled filter
12. 🔲 Clock divider/multiplier
13. 🔲 Analog mixer
14. 🔲 Additional voices/synth engines

---

## Parts Shopping List (Starter Kit)

**For Sequencer + Basic Effects**:
- 2x CD4017 ($1-2)
- 1x PT2399 delay module ($5-10)
- 1x 6N138 optocoupler + MIDI jack ($2-3)
- Assorted resistors/caps ($5)
- 3-5x DPDT switches for bypass ($5-10)
- Breadboard for prototyping ($5)
- Patch cables/wire ($5)

**Total**: ~$30-40 to get sequencer + delay + MIDI working

---

## Technical Notes

**Signal Levels**:
- PCM5102 output: ~2.1Vrms line level
- Most guitar pedals expect: instrument level (~0.5V) or line level
- May need level adjustment between stages

**Power Distribution**:
- Effects need clean 5V or 9V
- Share Teensy 5V rail (check current limits)
- Consider separate filtered supply for analog sections

**Grounding**:
- Star grounding to avoid ground loops
- Keep digital and analog grounds separate until final common point
- Watch for hum/noise with long cables

**Teensy Available Resources** (for expansion):
- Analog inputs: A4-A9 still free (6 more CV inputs!)
- Digital I/O: Pins 5, 6, 10-13, 22-39 (plenty!)
- PWM outputs: Multiple (for CV out, LED indicators)
- Serial ports: 3 more (MIDI, debug, etc.)
