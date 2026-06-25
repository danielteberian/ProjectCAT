/*
 * ProjectCatDisplay.h
 * --------------------
 * Single SSD1306 (128x64, I2C) status display for Project Cat on Teensy 4.1.
 * No mux -- direct on the Teensy's primary I2C bus (pins 18/19).
 *
 * Layout follows the established two-zone convention:
 *   Yellow top 16px zone  -> header only (page name, REC indicator)
 *   Blue lower 48px zone  -> page content
 *
 * Pages (cycle with encoder press, per project convention):
 *   0: Live   - waveform-ish level meter + active note
 *   1: Tone   - cutoff / resonance / drive
 *   2: Env    - attack / decay
 *   3: Lo-fi  - sample-rate-reduce / bitcrush / xor / fold
 *   4: REC    - recording status, elapsed time, free space
 */

#ifndef PROJECT_CAT_DISPLAY_H
#define PROJECT_CAT_DISPLAY_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AudioSynthProjectCat.h"

class ProjectCatDisplay {
public:
  static const uint8_t NUM_PAGES = 6;  // Added waveform page
  static const uint8_t SCOPE_WIDTH = 128;
  static const uint8_t SCOPE_SAMPLES = 128;

  ProjectCatDisplay() : oled(128, 64, &Wire, -1), page(0) {
    for (int i = 0; i < SCOPE_SAMPLES; i++) {
      scopeBuffer[i] = 0;
    }
    scopeIndex = 0;
  }

  bool begin() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) return false;
    oled.setTextColor(SSD1306_WHITE);
    oled.cp437(true);
    return true;
  }

  void nextPage() { page = (page + 1) % NUM_PAGES; }
  uint8_t currentPage() const { return page; }

  void draw(AudioSynthProjectCat &voice, bool recording, float recSeconds, uint32_t freeMB) {
    oled.clearDisplay();
    drawHeader(recording);
    switch (page) {
      case 0: drawLive(voice, recording);      break;
      case 1: drawTone(voice);                  break;
      case 2: drawEnv(voice);                   break;
      case 3: drawLofi(voice);                  break;
      case 4: drawRec(recording, recSeconds, freeMB); break;
      case 5: drawScope();                      break;  // New waveform page
    }
    oled.display();
  }

  // Call this from loop to update scope buffer with latest sample
  void updateScope(float sample) {
    // Normalize sample to 0-48 range for display (blue zone is 48px tall)
    int16_t y = (int16_t)((sample + 1.0f) * 24.0f);  // -1..1 -> 0..48
    if (y < 0) y = 0;
    if (y > 47) y = 47;

    scopeBuffer[scopeIndex] = y;
    scopeIndex = (scopeIndex + 1) % SCOPE_SAMPLES;
  }

private:
  Adafruit_SSD1306 oled;
  uint8_t page;

  static const char *pageName(uint8_t p) {
    switch (p) {
      case 0: return "LIVE";
      case 1: return "TONE";
      case 2: return "ENV";
      case 3: return "LO-FI";
      case 4: return "REC";
      case 5: return "SCOPE";  // New waveform visualization page
      default: return "";
    }
  }

  // Yellow zone: y=0..15. Header text only.
  void drawHeader(bool recording) {
    oled.setTextSize(1);
    oled.setCursor(0, 4);
    oled.print(pageName(page));
    if (recording) {
      oled.setCursor(104, 4);
      oled.print("REC");
    }
    oled.drawFastHLine(0, 15, 128, SSD1306_WHITE);
  }

  void bar(int x, int y, int w, int h, float norm) {
    oled.drawRect(x, y, w, h, SSD1306_WHITE);
    int fill = (int)(norm * (w - 2));
    if (fill > 0) oled.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
  }

  // Blue zone: y=18..63
  void drawLive(AudioSynthProjectCat &voice, bool recording) {
    oled.setCursor(0, 22);
    oled.print(voice.active() ? "Note: ON " : "Note: -- ");
    oled.print(trigShown ? "[TRIG]" : "");
    oled.setCursor(0, 34);
    oled.print("Cut:");  oled.print((int)(cutoffShown * 100));
    oled.print(" Drv:"); oled.print((int)(driveShown * 100));
    oled.setCursor(0, 44);
    oled.print("Dec:");  oled.print((int)(decayShown * 100));
    oled.print(" Pitch:"); oled.print((int)pitchShown);
    oled.setCursor(0, 56);
    if (effectNameShown) {
      oled.print("FX:");
      oled.print(effectNameShown);
    } else {
      oled.print(recording ? "RECORDING" : "D9=Sound Enc=FX");
    }
  }

  void drawTone(AudioSynthProjectCat &voice) {
    oled.setCursor(0, 20); oled.print("Cutoff");
    bar(60, 20, 64, 8, cutoffShown);
    oled.setCursor(0, 32); oled.print("Drive");
    bar(60, 32, 64, 8, driveShown);
    oled.setCursor(0, 44); oled.print("Res");
    bar(60, 44, 64, 8, resShown);
  }

  void drawEnv(AudioSynthProjectCat &voice) {
    oled.setCursor(0, 20); oled.print("Decay");
    bar(60, 20, 64, 8, decayShown);
    oled.setCursor(0, 36); oled.print("Pitch knob sets");
    oled.setCursor(0, 48); oled.print("base note freq");
  }

  void drawLofi(AudioSynthProjectCat &voice) {
    oled.setCursor(0, 20); oled.print("SR reduce / bits");
    oled.setCursor(0, 36); oled.print("XOR / fold (menu)");
    oled.setCursor(0, 50); oled.print("Phase 2: encoder menu");
  }

  void drawRec(bool recording, float secs, uint32_t freeMB) {
    oled.setCursor(0, 22);
    if (recording) {
      oled.print("Recording: ");
      oled.print(secs, 1);
      oled.print("s");
    } else {
      oled.print("Idle");
    }
    oled.setCursor(0, 38);
    oled.print("SD free: ");
    oled.print(freeMB);
    oled.print(" MB");
  }

  void drawScope() {
    // Draw live waveform visualization
    // Blue zone: y=18..63 (48px tall)
    const int centerY = 18 + 24;  // Center line of display area

    // Draw center reference line
    oled.drawFastHLine(0, centerY, 128, SSD1306_WHITE);

    // Draw waveform
    for (int x = 0; x < 127; x++) {
      int idx1 = (scopeIndex + x) % SCOPE_SAMPLES;
      int idx2 = (scopeIndex + x + 1) % SCOPE_SAMPLES;

      int y1 = 18 + scopeBuffer[idx1];
      int y2 = 18 + scopeBuffer[idx2];

      oled.drawLine(x, y1, x + 1, y2, SSD1306_WHITE);
    }

    // Show current waveform name
    oled.setCursor(0, 56);
    oled.setTextSize(1);
    if (waveformNameShown) {
      oled.print("Wave: ");
      oled.print(waveformNameShown);
    }
  }

  // Private buffer for scope display
  int16_t scopeBuffer[SCOPE_SAMPLES];
  uint8_t scopeIndex;

public:
  // Cached values for the bar displays, set from loop() each frame so the
  // display module doesn't need direct getters into the voice for params
  // that aren't exposed as read accessors.
  float cutoffShown = 0.0f, driveShown = 0.0f, resShown = 0.0f, decayShown = 0.0f;
  float pitchShown = 0.0f;
  bool  trigShown = false;
  const char* waveformNameShown = nullptr;  // For scope page
  const char* effectNameShown = nullptr;    // For live page
};

#endif // PROJECT_CAT_DISPLAY_H
