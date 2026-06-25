# Project Cat - Hardware Wiring Guide

Complete wiring guide for the Teensy 4.1 Project Cat synthesizer.

---

## 📋 Components Needed

### Required:
- **Teensy 4.1** microcontroller
- **PCM5102 I2S DAC** module
- **LM386 amplifier** module (or similar)
- **8Ω speaker** (or headphone jack)
- **SSD1306 OLED display** (128x64, I2C)
- **Power supply** (USB or 5V DC)

### Optional:
- **4x 10kΩ potentiometers** (linear taper recommended)
- **EC11 rotary encoder** with push button
- **2x tactile buttons** (for D8 trigger and D9 sound select)
- **MicroSD card** (for recording, if re-enabled)

---

## 🔌 Wiring Connections

### 1. Audio Output (PCM5102 DAC → LM386 → Speaker)

#### PCM5102 to Teensy 4.1:
```
PCM5102 Pin    →    Teensy Pin
─────────────────────────────────
VIN (or VCC)   →    3.3V
GND            →    GND
BCLK (BCK)     →    Pin 21 (I2S_BCLK)
LRCLK (LRC)    →    Pin 20 (I2S_LRCLK / I2S_WS)
DIN (DATA)     →    Pin 7  (I2S_TX)
SCK            →    GND (set to slave mode)
XSMT (or FMT)  →    3.3V (unmute / I2S format)
```

**Notes:**
- Some PCM5102 modules have different pin labels (BCK/LRCK/DIN)
- Connecting SCK to GND puts the DAC in slave mode (Teensy provides clock)
- XSMT to 3.3V unmutes the output

#### PCM5102 to LM386:
```
PCM5102        →    LM386
─────────────────────────────
OUT (or LOUT)  →    Input Pin (usually pin 3 via 10μF capacitor)
GND            →    GND
```

#### LM386 to Speaker:
```
LM386          →    Speaker
─────────────────────────────
Output         →    Speaker + (via 100μF capacitor)
GND            →    Speaker -
```

**Volume Control (optional):**
- Add a 10kΩ potentiometer between LM386 input and ground to control volume

---

### 2. OLED Display (SSD1306)

#### OLED to Teensy 4.1:
```
OLED Pin       →    Teensy Pin
─────────────────────────────────
VCC            →    3.3V
GND            →    GND
SDA            →    Pin 18 (I2C SDA)
SCL            →    Pin 19 (I2C SCL)
```

**Notes:**
- I2C address is typically 0x3C (set in code)
- No pull-up resistors needed (usually built into module)
- If display doesn't work, try 0x3D address

---

### 3. Potentiometers (A0-A3)

**Use 10kΩ linear taper pots for smooth control**

#### Pot Wiring (all 4 pots use same pattern):

```
Potentiometer Pin    →    Connection
──────────────────────────────────────
Pin 1 (CCW)          →    GND
Pin 2 (Wiper/Center) →    Teensy Analog Pin
Pin 3 (CW)           →    3.3V
```

#### Specific Assignments:
```
Function       Teensy Pin    Parameter Controlled
────────────────────────────────────────────────────
Cutoff         A0 (Pin 14)   Filter brightness (20Hz-18kHz)
Drive          A1 (Pin 15)   Saturation/warmth (0-100%)
Decay          A2 (Pin 16)   Envelope decay time (20ms-2s)
Pitch          A3 (Pin 17)   Base note pitch (55Hz-880Hz, 4 octaves)
```

**Visual Diagram (single pot):**
```
       3.3V ──┐
              │
          ┌───┴───┐
          │  POT  │ ← 10kΩ linear
          │       │
          └───┬───┘
              │
      Wiper ──┴──→ to Teensy (A0/A1/A2/A3)

       GND ────────
```

**Notes:**
- If pots are not wired, code will show noisy values (ignore or comment out pot code)
- Values range from 0-4095 (12-bit ADC)
- Smoothing is applied in software to reduce jitter

---

### 4. Buttons

#### Trigger Button (D8):
```
Button          →    Teensy
──────────────────────────────
Pin 1           →    Pin 8 (D8)
Pin 2           →    GND
```

**Notes:**
- INPUT_PULLUP mode, so press = LOW
- Used to trigger/gate notes (press and hold)

#### Sound Select Button (D9):
```
Button          →    Teensy
──────────────────────────────
Pin 1           →    Pin 9 (D9)
Pin 2           →    GND
```

**Notes:**
- INPUT_PULLUP mode
- Cycles through 16 sounds (waveforms, drums, synths)

---

### 5. Rotary Encoder (Optional)

#### EC11 Encoder to Teensy:
```
Encoder Pin    →    Teensy Pin
─────────────────────────────────
A (CLK)        →    Pin 2
B (DT)         →    Pin 3
SW (Button)    →    Pin 4
GND (C)        →    GND
```

**Notes:**
- All pins use INPUT_PULLUP
- Encoder cycles OLED display pages
- Push button also advances pages

---

### 6. MicroSD Card (Built-in on Teensy 4.1)

The Teensy 4.1 has a built-in microSD slot on the bottom of the board.

**SD Card Pins (SDIO - internal, no wiring needed):**
- Just insert a microSD card formatted as **exFAT** (for cards >32GB) or **FAT32**
- Used for WAV recording (currently disabled in code)

**Notes:**
- Recording is disabled by default (commented out)
- To re-enable: uncomment recording code blocks in .ino file
- Card must be formatted and properly inserted

---

## 🔋 Power Supply

### USB Power (Easiest):
```
USB Cable → Teensy 4.1 USB port
```
- Provides 5V power
- Powers Teensy, PCM5102, OLED
- LM386 may need separate 5V supply for higher volume

### External 5V Supply:
```
5V DC → Teensy VIN pin
GND   → Teensy GND pin
```

**Current Requirements:**
- Teensy 4.1: ~100mA
- PCM5102: ~30mA
- OLED: ~20mA
- LM386: 50-700mA (depending on volume)
- **Total: ~200mA minimum, 1A recommended**

---

## 📐 Complete Wiring Diagram (Text)

```
                    TEENSY 4.1
                   ┌──────────┐
        GND  ──────┤ GND      │
        3.3V ──────┤ 3V3      │
                   │          │
PCM5102 BCLK ──────┤ 21       │
PCM5102 LRCLK ─────┤ 20       │
OLED SCL ──────────┤ 19       │
OLED SDA ──────────┤ 18       │
POT Pitch ─────────┤ A3 (17)  │
POT Decay ─────────┤ A2 (16)  │
POT Drive ─────────┤ A1 (15)  │
POT Cutoff ────────┤ A0 (14)  │
                   │          │
Trigger Button ────┤ 8  (D8)  │
Sound Button ──────┤ 9  (D9)  │
PCM5102 DIN ───────┤ 7        │
Encoder SW ────────┤ 4        │
Encoder B ─────────┤ 3        │
Encoder A ─────────┤ 2        │
                   └──────────┘

        PCM5102              LM386           SPEAKER
        ┌─────┐             ┌─────┐          ┌───┐
3.3V ───┤ VIN │             │     │          │   │
GND  ───┤ GND │─────────────┤ GND │──────────┤ - │
BCLK ───┤ BCK │             │     │          │   │
LRCK ───┤ LRC │    ┌────────┤ IN  │          │   │
DIN  ───┤ DIN │    │  10μF  │     │   100μF  │   │
GND  ───┤ SCK │    └────────┤     ├──────────┤ + │
3.3V ───┤ XMT │             │ OUT │          │   │
        │ OUT ├─────────────┤     │          └───┘
        └─────┘             └─────┘

        OLED                 POTENTIOMETER (x4)
        ┌────┐                  ┌─────┐
3.3V ───┤ VCC│           3.3V ──┤  3  │
GND  ───┤ GND│           GND  ──┤  1  │
SDA  ───┤ SDA│           A0-A3 ─┤  2  │ (wiper)
SCL  ───┤ SCL│                  └─────┘
        └────┘
```

---

## 🧪 Testing

### 1. Power Test:
- Plug in USB
- Teensy LED should light up
- OLED should display boot messages

### 2. Audio Test:
- Press D8 button
- Should hear tone from speaker
- Press D9 to change sounds

### 3. Pot Test:
- Turn pots while holding D8
- Sound should change (pitch, brightness, decay)
- Watch Serial Monitor for pot values (should be 0-4095)

### 4. Display Test:
- Rotate encoder
- OLED pages should cycle (LIVE/TONE/ENV/LOFI/REC/SCOPE)
- SCOPE page shows live waveform

---

## ⚠️ Troubleshooting

### No Sound:
- Check PCM5102 wiring (especially BCLK, LRCLK, DIN)
- Verify LM386 power and output connections
- Check speaker polarity
- Ensure XSMT pin is tied to 3.3V (unmute)

### Noisy/Distorted Sound:
- Add decoupling capacitors (0.1μF) near ICs
- Check power supply quality
- Reduce LM386 gain/volume

### OLED Not Working:
- Verify I2C address (0x3C or 0x3D)
- Check SDA/SCL connections
- Try different I2C pull-up resistor values (4.7kΩ)

### Pots Not Responding:
- Check wiper connection to analog pin
- Verify 3.3V and GND connections
- Watch Serial Monitor for changing values

### Recording Not Working:
- Recording is disabled by default
- Format SD card as exFAT or FAT32
- Uncomment recording code blocks in .ino file

---

## 📚 Additional Resources

- **Teensy 4.1 Pinout:** https://www.pjrc.com/store/teensy41.html
- **PCM5102 Datasheet:** Search "PCM5102A datasheet"
- **Adafruit SSD1306 Guide:** https://learn.adafruit.com/monochrome-oled-breakouts

---

**Built with ❤️ for Project Cat**

*Last updated: 2026-06-20*
