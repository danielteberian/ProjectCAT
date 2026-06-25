/*
 * ProjectCat_Teensy.ino - SAFE MODE VERSION
 * ---------------------
 * Project Cat -- Teensy 4.1, single-OLED build.
 *
 * CURRENT MODE: MINIMAL SAFE BOOT
 * If this works, we'll add features back one at a time.
 *
 * Hardware:
 *   - PCM5102 I2S DAC -> LM386 -> 8ohm speaker
 *   - 1x SSD1306 128x64 OLED, direct I2C (no mux)
 *   - EC11 rotary encoder (page select + press)
 *   - 4 pots: A0 cutoff, A1 drive, A2 decay, A3 pitch
 *   - D8 trigger button (note on/off)
 *   - D9 dedicated SAVE/EXPORT button -> records DAC output to SD as WAV
 *   - Built-in SDIO microSD (BUILTIN_SDCARD)
 *
 * Audio graph:
 *   AudioSynthProjectCat -> AudioOutputI2S        (live monitoring, L ch)
 *                        -> AudioRecordQueue       (tapped for WAV export)
 *
 * PCM5102 wiring (I2S1):
 *   BCLK->21  LRCLK->20  DIN->7  SCK->GND  XSMT->3V3 (unmute)
 *
 * OLED wiring (I2C0):
 *   SDA->18  SCL->19  VCC->3V3  GND->GND
 *
 * EC11 wiring:
 *   A->2  B->3  SW->4  (common->GND, INPUT_PULLUP on all three)
 *
 * Save/export behavior:
 *   Press D9 once  -> start recording to /sample###.wav (auto-incremented)
 *   Press D9 again -> stop recording, finalize WAV header
 *   OLED page 4 (REC) shows live elapsed time and SD free space.
 *
 * Build: Arduino IDE 2.3.0+ + Teensyduino. Board: Teensy 4.1.
 * USB Type: Serial + MIDI. Libraries: Audio, SD, Wire, Adafruit_GFX,
 * Adafruit_SSD1306 (all bundled with Teensyduino except Adafruit libs).
 */

#include <Audio.h>
#include <SD.h>
#include <Wire.h>
#include "AudioSynthProjectCat.h"
#include "ProjectCatDisplay.h"
#include "WavWriter.h"

// ---------------------------------------------------------------------------
// Audio graph
// ---------------------------------------------------------------------------
AudioSynthProjectCat voice;
AudioOutputI2S        i2sOut;

AudioConnection patchOut(voice, 0, i2sOut, 0);     // live monitor, left ch
AudioConnection patchOutR(voice, 0, i2sOut, 1);    // right channel too

ProjectCatDisplay display;

// ============================================================================
// WAV RECORDING SYSTEM - RE-ENABLED
// ============================================================================
// The AudioRecordQueue + WavWriter allows recording synth output to SD card.
// Press D9 to toggle recording on/off.
// Files are saved as /sample000.wav, /sample001.wav, etc.
// ============================================================================
AudioRecordQueue      recQueue;
AudioConnection patchRec(voice, 0, recQueue, 0);
WavWriter         wav(recQueue);

// ---- pins ----
const int POT_CUTOFF = A0;
const int POT_DRIVE  = A1;
const int POT_DECAY  = A2;
const int POT_PITCH  = A3;
const int BTN_TRIG   = 8;
const int BTN_SOUND  = 9;     // sound/preset selection button
const int BTN_REC    = 10;    // recording toggle button
const int ENC_A      = 2;
const int ENC_B      = 3;
const int ENC_SW     = 4;

// ---- encoder state (detent-filtered quadrature, same pattern as ESP32 build) ----
volatile int8_t encSubStep = 0;
int8_t lastEncState = 0;

// ---- button edge tracking with debounce ----
bool lastTrig = false;
bool lastSound = false;
bool lastRec = false;
bool lastEncSw = false;
elapsedMillis trigDebounce;
elapsedMillis soundDebounce;
elapsedMillis recDebounce;
const int DEBOUNCE_MS = 20;  // Debounce time in milliseconds

// ---- waveform/sound selection (cycle with D9 button since recording is off) ----
int currentWaveform = 0;

// Sound names (waveforms + presets)
const char* waveformNames[] = {
  // Basic waveforms (0-5)
  "SAW", "SINE", "SQUARE", "TRI", "NOISE", "KARPLUS",

  // Drums (6-15)
  "KICK", "SNARE", "HIHAT", "CLAP", "TOM", "COWBELL",
  "RIMSHOT", "CYMBAL", "WOODBLK", "SHAKER",

  // Synths (16-27)
  "BASS", "LEAD", "PAD", "BELL", "PLUCK", "ORGAN",
  "BRASS", "STRINGS", "ARP", "CHOIR", "RHODES", "WOBBLE",

  // Special FX (28-31)
  "LASER", "BLEEP", "ZAP", "GLITCH"
};
const int NUM_WAVEFORMS = 32;

// ---- Effects toggles (encoder button to cycle) ----
bool fxBitcrush = false;
bool fxDistortion = false;
bool fxDelay = false;
int currentEffect = 0;  // 0=none, 1=bitcrush, 2=distortion, 3=delay, 4=FM, 5=LFO
const char* effectNames[] = {"OFF", "BITCRUSH", "DISTORT", "DELAY", "FM", "LFO"};

// Last rendered sample for scope visualization
float lastSample = 0.0f;
elapsedMillis scopeUpdate;

// ---- pot smoothing (exponential filter) ----
float smoothCutoff = 0.0f;
float smoothDrive  = 0.0f;
float smoothDecay  = 0.0f;
float smoothPitch  = 0.0f;
const float POT_ALPHA = 0.05f;  // lower = smoother (0.01-0.1 typical)

elapsedMillis uiRefresh;
elapsedMillis cpuReport;
elapsedMillis debugReport;
elapsedMicros loopTimer;
int fileIndex = 0;

// ---------------------------------------------------------------------------
// Encoder ISR-lite: poll-based quadrature with detent filter.
// Ported from the ESP32 build's readEncoder() -- only commits a step when
// the encoder returns to its rest state (A=1,B=1) after a full +-2 sub-step
// swing, which eliminates the phantom-rotation glitches you hit before.
// ---------------------------------------------------------------------------
int8_t pollEncoderStep() {
  static const int8_t table[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
  };
  int a = digitalRead(ENC_A);
  int b = digitalRead(ENC_B);
  int8_t state = (a << 1) | b;
  int8_t idx = (lastEncState << 2) | state;
  encSubStep += table[idx & 0x0F];
  lastEncState = state;

  int8_t result = 0;
  if (state == 0b11) {           // rest position
    if (encSubStep >= 2)  result =  1;
    if (encSubStep <= -2) result = -1;
    encSubStep = 0;
  }
  return result;
}

// ---------------------------------------------------------------------------
// SD helpers
// ---------------------------------------------------------------------------
// ============================================================================
// SD FORMAT FUNCTION - COMMENTED OUT (recording system disabled)
// ============================================================================
/*
bool formatSDCard() {
  Serial.println("\n=================================");
  Serial.println("FORMATTING SD CARD...");
  Serial.println("This will ERASE ALL DATA!");
  Serial.println("=================================\n");

  Serial.println("Attempting format...");
  Serial.println("(This may take 10-30 seconds for large cards)");
  delay(100);

  // Direct format using SdFat
  if (!SD.sdfs.format()) {
    Serial.println("\nFORMAT FAILED!");
    Serial.println("Possible reasons:");
    Serial.println("- Card is write-protected (check physical switch)");
    Serial.println("- Card is corrupted or faulty");
    Serial.println("- Card is too large (>32GB may need computer)");
    Serial.println("\nTry formatting on your computer as FAT32 instead");
    return false;
  }

  Serial.println("\nFORMAT COMPLETE!");
  Serial.println("Re-initializing SD card...");

  delay(500);

  // Re-initialize with BUILTIN_SDCARD
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("WARNING: SD re-init failed after format");
    Serial.println("BUILTIN_SDCARD SDIO interface not responding");
    Serial.println("Try pressing RESET button or power cycle Teensy");
    return false;
  }

  Serial.println("SUCCESS! SD card is ready to use");
  Serial.print("Free space: ");
  Serial.print(sdFreeMB());
  Serial.println(" MB\n");
  return true;
}
*/

// ============================================================================
// SOUND PRESET FUNCTIONS
// ============================================================================
void applySoundPreset(int presetIndex) {
  // First 6 are basic waveforms, rest are preset sounds
  if (presetIndex < 6) {
    // Basic waveforms - just change waveform, keep other settings
    voice.setWaveform((AudioSynthProjectCat::Waveform)presetIndex);
  } else {
    // Sound presets - configure multiple parameters
    switch (presetIndex) {
      // ==== DRUM SOUNDS ====
      case 6:  // KICK - deep bass drum
        voice.setWaveform(AudioSynthProjectCat::WAVE_SINE);
        voice.setCutoff(0.2f);    // Very low cutoff for thump
        voice.setResonance(0.3f); // Some resonance for body
        voice.setDrive(0.8f);     // Heavy drive for punch
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.15f);    // Short decay (150ms)
        break;

      case 7:  // SNARE - snappy snare drum
        voice.setWaveform(AudioSynthProjectCat::WAVE_NOISE);
        voice.setCutoff(0.6f);    // Mid cutoff
        voice.setResonance(0.7f); // High resonance for snap
        voice.setDrive(0.5f);     // Moderate drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.08f);    // Very short decay (80ms)
        break;

      case 8:  // HIHAT - crisp hi-hat
        voice.setWaveform(AudioSynthProjectCat::WAVE_NOISE);
        voice.setCutoff(0.95f);   // Very high cutoff for brightness
        voice.setResonance(0.2f); // Low resonance
        voice.setDrive(0.3f);     // Light drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.04f);    // Super short decay (40ms)
        break;

      case 9:  // CLAP - hand clap
        voice.setWaveform(AudioSynthProjectCat::WAVE_NOISE);
        voice.setCutoff(0.7f);    // Mid-high cutoff
        voice.setResonance(0.5f); // Mid resonance
        voice.setDrive(0.6f);     // Good drive for snap
        voice.setAttack(0.002f);  // Tiny attack
        voice.setDecay(0.06f);    // Short decay (60ms)
        break;

      case 10:  // TOM - tom drum
        voice.setWaveform(AudioSynthProjectCat::WAVE_SINE);
        voice.setCutoff(0.4f);    // Low-mid cutoff
        voice.setResonance(0.6f); // High resonance for tone
        voice.setDrive(0.5f);     // Moderate drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.25f);    // Medium decay (250ms)
        break;

      case 11:  // COWBELL - classic cowbell
        voice.setWaveform(AudioSynthProjectCat::WAVE_SQUARE);
        voice.setCutoff(0.85f);   // High cutoff for brightness
        voice.setResonance(0.8f); // Very high resonance for bell tone
        voice.setDrive(0.4f);     // Moderate drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.3f);     // Medium-long decay
        break;

      // ==== SYNTH SOUNDS ====
      case 12:  // BASS - deep, warm bass
        voice.setWaveform(AudioSynthProjectCat::WAVE_SAW);
        voice.setCutoff(0.3f);    // Low cutoff for darkness
        voice.setResonance(0.5f); // Mid resonance for character
        voice.setDrive(0.6f);     // More drive for warmth
        voice.setDecay(0.5f);     // Medium decay
        break;

      case 13:  // LEAD - bright, cutting lead
        voice.setWaveform(AudioSynthProjectCat::WAVE_SAW);
        voice.setCutoff(0.85f);   // High cutoff for brightness
        voice.setResonance(0.3f); // Some resonance
        voice.setDrive(0.4f);     // Moderate drive
        voice.setDecay(0.4f);     // Medium decay
        break;

      case 14:  // PAD - soft, smooth pad
        voice.setWaveform(AudioSynthProjectCat::WAVE_TRI);
        voice.setCutoff(0.6f);    // Mid cutoff
        voice.setResonance(0.1f); // Low resonance for smoothness
        voice.setDrive(0.2f);     // Light drive
        voice.setDecay(1.0f);     // Long decay
        break;

      case 15:  // BELL - bell-like, metallic
        voice.setWaveform(AudioSynthProjectCat::WAVE_SINE);
        voice.setCutoff(0.9f);    // High cutoff for brightness
        voice.setResonance(0.6f); // High resonance for ring
        voice.setDrive(0.3f);     // Light drive
        voice.setDecay(0.8f);     // Long decay for ring-out
        break;

      // ==== MORE DRUMS ====
      case 16:  // RIMSHOT - sharp, metallic rim hit
        voice.setWaveform(AudioSynthProjectCat::WAVE_SQUARE);
        voice.setCutoff(0.75f);   // Mid-high cutoff
        voice.setResonance(0.9f); // Very high resonance for metallic tone
        voice.setDrive(0.7f);     // High drive for snap
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.05f);    // Very short decay (50ms)
        break;

      case 17:  // CYMBAL - bright, splashy crash
        voice.setWaveform(AudioSynthProjectCat::WAVE_NOISE);
        voice.setCutoff(1.0f);    // Maximum cutoff for extreme brightness
        voice.setResonance(0.4f); // Moderate resonance for character
        voice.setDrive(0.4f);     // Moderate drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.4f);     // Medium decay (400ms)
        break;

      case 18:  // WOODBLOCK - hollow, wooden tone
        voice.setWaveform(AudioSynthProjectCat::WAVE_TRI);
        voice.setCutoff(0.55f);   // Mid cutoff
        voice.setResonance(0.85f); // Very high resonance for hollow tone
        voice.setDrive(0.6f);     // Good drive for punch
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.07f);    // Short decay (70ms)
        break;

      case 19:  // SHAKER - high, rattling noise
        voice.setWaveform(AudioSynthProjectCat::WAVE_NOISE);
        voice.setCutoff(0.88f);   // High cutoff
        voice.setResonance(0.15f); // Low resonance
        voice.setDrive(0.25f);    // Light drive
        voice.setAttack(0.002f);  // Tiny attack
        voice.setDecay(0.12f);    // Short decay (120ms)
        break;

      // ==== MORE SYNTHS ====
      case 20:  // PLUCK - pizzicato string pluck
        voice.setWaveform(AudioSynthProjectCat::WAVE_KS);  // Karplus-Strong
        voice.setCutoff(0.65f);   // Mid-high cutoff
        voice.setResonance(0.3f); // Some resonance
        voice.setDrive(0.2f);     // Very light drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.6f);     // Medium-long decay
        break;

      case 21:  // ORGAN - full-bodied organ tone
        voice.setWaveform(AudioSynthProjectCat::WAVE_SINE);
        voice.setCutoff(0.75f);   // Mid-high cutoff
        voice.setResonance(0.25f); // Low resonance for smoothness
        voice.setDrive(0.5f);     // Moderate drive for body
        voice.setDecay(0.8f);     // Long decay
        break;

      case 22:  // BRASS - bright, punchy brass
        voice.setWaveform(AudioSynthProjectCat::WAVE_SAW);
        voice.setCutoff(0.72f);   // Mid-high cutoff
        voice.setResonance(0.45f); // Moderate resonance for punch
        voice.setDrive(0.65f);    // High drive for brassy tone
        voice.setDecay(0.5f);     // Medium decay
        break;

      case 23:  // STRINGS - smooth, lush string pad
        voice.setWaveform(AudioSynthProjectCat::WAVE_SAW);
        voice.setCutoff(0.55f);   // Mid cutoff
        voice.setResonance(0.15f); // Low resonance for smoothness
        voice.setDrive(0.35f);    // Moderate drive
        voice.setAttack(0.05f);   // Slow attack (50ms) for swell
        voice.setDecay(1.2f);     // Long decay
        break;

      case 24:  // ARP - sharp, bright arpeggio sound
        voice.setWaveform(AudioSynthProjectCat::WAVE_SQUARE);
        voice.setCutoff(0.8f);    // High cutoff
        voice.setResonance(0.2f); // Low resonance
        voice.setDrive(0.3f);     // Light drive
        voice.setAttack(0.002f);  // Very fast attack
        voice.setDecay(0.2f);     // Short decay for staccato
        break;

      case 25:  // CHOIR - soft, vocal-like pad
        voice.setWaveform(AudioSynthProjectCat::WAVE_TRI);
        voice.setCutoff(0.5f);    // Mid cutoff
        voice.setResonance(0.35f); // Moderate resonance for formant
        voice.setDrive(0.25f);    // Light drive
        voice.setAttack(0.08f);   // Slow attack (80ms)
        voice.setDecay(1.5f);     // Very long decay
        break;

      case 26:  // RHODES - electric piano tone
        voice.setWaveform(AudioSynthProjectCat::WAVE_SINE);
        voice.setCutoff(0.68f);   // Mid-high cutoff
        voice.setResonance(0.4f); // Moderate resonance for bell-like tone
        voice.setDrive(0.45f);    // Moderate drive
        voice.setDecay(0.7f);     // Medium-long decay
        break;

      case 27:  // WOBBLE - dubstep-style wobble bass
        voice.setWaveform(AudioSynthProjectCat::WAVE_SAW);
        voice.setCutoff(0.25f);   // Low cutoff (LFO will modulate this)
        voice.setResonance(0.7f); // High resonance for emphasis
        voice.setDrive(0.75f);    // High drive for grit
        voice.setDecay(0.6f);     // Medium decay
        // Note: Enable LFO effect for full wobble
        break;

      // ==== SPECIAL FX ====
      case 28:  // LASER - sci-fi laser zap
        voice.setWaveform(AudioSynthProjectCat::WAVE_SAW);
        voice.setCutoff(0.95f);   // Very high cutoff
        voice.setResonance(0.5f); // Moderate resonance
        voice.setDrive(0.6f);     // Good drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.15f);    // Short decay with pitch sweep
        break;

      case 29:  // BLEEP - digital beep/bloop
        voice.setWaveform(AudioSynthProjectCat::WAVE_SQUARE);
        voice.setCutoff(0.85f);   // High cutoff
        voice.setResonance(0.1f); // Very low resonance
        voice.setDrive(0.2f);     // Light drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.08f);    // Very short decay
        break;

      case 30:  // ZAP - electric discharge sound
        voice.setWaveform(AudioSynthProjectCat::WAVE_NOISE);
        voice.setCutoff(0.7f);    // Mid-high cutoff
        voice.setResonance(0.6f); // High resonance for tone
        voice.setDrive(0.9f);     // Very high drive for aggression
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.06f);    // Very short decay (60ms)
        break;

      case 31:  // GLITCH - broken, digital artifact
        voice.setWaveform(AudioSynthProjectCat::WAVE_SQUARE);
        voice.setCutoff(0.45f);   // Mid cutoff
        voice.setResonance(0.8f); // Very high resonance for digital ring
        voice.setDrive(0.85f);    // Very high drive
        voice.setAttack(0.001f);  // Instant attack
        voice.setDecay(0.1f);     // Short decay
        // Note: Bitcrush or XOR effects work great with this
        break;
    }
  }
}

// ============================================================================
// SD HELPER FUNCTIONS
// ============================================================================
String nextSampleFilename() {
  char buf[24];
  while (true) {
    snprintf(buf, sizeof(buf), "/sample%03d.wav", fileIndex);
    if (!SD.exists(buf)) return String(buf);
    fileIndex++;
    if (fileIndex > 999) fileIndex = 0;
  }
}

uint32_t sdFreeMB() {
  if (!SD.sdfs.vol()) return 0;
  uint64_t freeBytes = (uint64_t)SD.sdfs.freeClusterCount() * SD.sdfs.bytesPerCluster();
  return (uint32_t)(freeBytes / (1024UL * 1024UL));
}

// ============================================================================
// RECORDING FUNCTIONS
// ============================================================================
void startRecording() {
  Serial.println("startRecording() called");

  // Start the record queue (allocates ~53 audio blocks)
  recQueue.begin();
  Serial.println("      Record queue started");

  String path = nextSampleFilename();
  Serial.print("Attempting to create file: ");
  Serial.println(path);

  if (wav.begin(path.c_str())) {
    Serial.print("SUCCESS! Recording to ");
    Serial.println(path);
  } else {
    Serial.println("FAILED! SD open failed -- check card present / formatted");
    Serial.println("Try formatting the SD card on your computer (FAT32 or exFAT)");
    Serial.println("Ensure SD card is properly inserted in Teensy 4.1 bottom slot");
    // If file open failed, stop the queue to free memory
    recQueue.end();
  }
}

void stopRecording() {
  wav.end();
  Serial.print("Saved. Bytes written: ");
  Serial.println(wav.bytesWritten());
  fileIndex++;

  // Stop the record queue (frees ~53 audio blocks)
  recQueue.end();
  Serial.println("      Record queue stopped (freed audio memory)");
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(3000);  // Give Serial plenty of time to connect

  // Send multiple messages to ensure something shows up
  Serial.println();
  Serial.println();
  Serial.println("##############################");
  Serial.println("### PROJECT CAT STARTING ###");
  Serial.println("##############################");
  Serial.println();
  Serial.print("Boot time: ");
  Serial.print(millis());
  Serial.println(" ms");
  Serial.println();

  Serial.println("[1/7] Setting up pins...");
  Serial.flush();  // Force output
  pinMode(BTN_TRIG, INPUT_PULLUP);
  pinMode(BTN_SOUND, INPUT_PULLUP);
  pinMode(BTN_REC, INPUT_PULLUP);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  Serial.println("      Pins OK");

  // Test pot readings at startup
  Serial.println("      Testing pot connections:");
  delay(10);  // Let ADC settle
  Serial.print("        A0 (Cutoff): ");
  Serial.println(analogRead(POT_CUTOFF));
  Serial.print("        A1 (Drive):  ");
  Serial.println(analogRead(POT_DRIVE));
  Serial.print("        A2 (Decay):  ");
  Serial.println(analogRead(POT_DECAY));
  Serial.print("        A3 (Pitch):  ");
  Serial.println(analogRead(POT_PITCH));
  Serial.println("      (If all same value, pots not wired. Values should be 0-4095)");
  Serial.flush();

  Serial.println("[2/7] Starting audio...");
  Serial.flush();
  AudioMemory(70);  // Enough for synth + RecordQueue when active (~53 blocks)
  analogReadResolution(12);
  Serial.println("      Audio memory: 70 blocks allocated");
  Serial.println("      Audio OK");
  Serial.flush();

  Serial.println("[3/7] Setting up voice...");
  Serial.flush();

  // ---- Recording system ----
  // NOTE: recQueue.begin() allocates ~53 audio blocks internally!
  // We'll only call begin() when actually starting recording to save memory
  // recQueue.begin();  // MOVED to startRecording() function
  Serial.println("      Record queue ready (will start on first recording)");

  // Basic settings
  currentWaveform = 0;  // Start with SAW
  voice.setWaveform(AudioSynthProjectCat::WAVE_SAW);

  // Re-enable FM and LFO (were disabled due to ESP32 drone issue - testing on Teensy)
  voice.setFM(false);        // Start disabled, will add toggle control
  voice.setFMRatio(2.0f);    // 2x carrier frequency (harmonic)
  voice.setFMDepth(0.3f);    // Moderate depth (was 0.5 on ESP32, reducing to test)

  voice.setLFO(false);       // Start disabled, will add toggle control
  voice.setLFORate(5.0f);    // 5 Hz vibrato
  voice.setLFODepth(0.05f);  // Subtle vibrato (was higher on ESP32)

  voice.setSustainMode(false);
  voice.setOutputGain(0.8f);

  // Explicitly disable lo-fi effects (to ensure clean sound)
  voice.setSampleRateReduce(0.0f);  // No sample rate reduction
  voice.setBitDepth(16.0f);          // Full 16-bit (no bitcrush)
  voice.setXorAmount(0.0f);          // No XOR distortion
  voice.setFold(0.0f);               // No wavefolding

  // Set reasonable defaults (pots will override these)
  voice.setCutoff(0.7f);             // Mid-high cutoff
  voice.setResonance(0.2f);          // Mild resonance
  voice.setDrive(0.3f);              // Light drive
  voice.setAttack(0.005f);           // Fast attack (5ms)
  voice.setDecay(0.4f);              // Medium decay

  Serial.println("      Voice configured:");
  Serial.println("        Waveform: SAW (D9 to change)");
  Serial.println("        Effects: OFF (encoder button to cycle)");
  Serial.println("        FM/LFO: Available via effects menu");
  Serial.println("        Pots: ENABLED for live control");
  Serial.flush();

  Serial.println("[4/7] Starting I2C...");
  Serial.flush();
  Wire.begin();
  Serial.println("      I2C OK");
  Serial.flush();

  Serial.println("[5/7] Initializing OLED...");
  Serial.flush();
  if (!display.begin()) {
    Serial.println("      OLED FAILED (continuing anyway)");
  } else {
    Serial.println("      OLED OK");
  }
  Serial.flush();

  // ---- SD card initialization ----
  Serial.println("[6/7] Initializing SD card...");
  Serial.println("      Using BUILTIN_SDCARD (SDIO interface)");
  Serial.println("      Ensure SD card is inserted in bottom slot of Teensy 4.1");
  Serial.flush();

  delay(500);

  bool sdOk = false;
  for (int attempt = 0; attempt < 5; attempt++) {
    if (attempt > 0) {
      Serial.print("      Retry #");
      Serial.print(attempt);
      Serial.println("...");
      delay(300);
    }

    if (SD.begin(BUILTIN_SDCARD)) {
      sdOk = true;
      Serial.println("      SD initialized via SDIO!");
      break;
    } else {
      Serial.print("      Attempt ");
      Serial.print(attempt + 1);
      Serial.println(" failed - SDIO init error");
    }
  }

  if (!sdOk) {
    Serial.println();
    Serial.println("      *** SD CARD INIT FAILED ***");
    Serial.println("      Possible causes:");
    Serial.println("        - No SD card inserted");
    Serial.println("        - Card not formatted (try FAT32 or exFAT)");
    Serial.println("        - Card contacts dirty/damaged");
    Serial.println("        - Card size not supported");
    Serial.println("      Synth will work, but recording disabled");
    Serial.println("      Continuing without SD...");
  } else {
    Serial.println("      SD OK!");
    uint32_t freeMB = sdFreeMB();
    if (freeMB > 0) {
      Serial.print("      Free space: ");
      Serial.print(freeMB);
      Serial.println(" MB");
    }
  }
  Serial.flush();

  Serial.println("[7/7] Final setup...");
  Serial.flush();
  lastEncState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
  Serial.println("      Done");
  Serial.flush();

  Serial.println();
  Serial.println("##############################");
  Serial.println("### BOOT COMPLETE ###");
  Serial.println("##############################");
  Serial.println();
  Serial.println("Project Cat Synth Ready!");
  Serial.println();
  Serial.println("Controls:");
  Serial.println("  D8 = Trigger sound (press & hold)");
  Serial.println("  D9 = Cycle sounds/presets");
  Serial.println("  D10 = Toggle WAV recording (saves to SD card)");
  Serial.println("  Encoder rotation = Cycle OLED pages");
  Serial.println("  Encoder button = Cycle effects (OFF/BITCRUSH/DISTORT/DELAY/FM/LFO)");
  Serial.println("  A0-A3 pots = Cutoff/Drive/Decay/Pitch (see WIRING.md)");
  Serial.println();
  Serial.println("Sounds (D9 to cycle - 32 total):");
  Serial.println("  Waveforms (0-5): SAW/SINE/SQUARE/TRI/NOISE/KARPLUS");
  Serial.println("  Drums (6-15): KICK/SNARE/HIHAT/CLAP/TOM/COWBELL/RIMSHOT/CYMBAL/WOODBLK/SHAKER");
  Serial.println("  Synths (16-27): BASS/LEAD/PAD/BELL/PLUCK/ORGAN/BRASS/STRINGS/ARP/CHOIR/RHODES/WOBBLE");
  Serial.println("  FX (28-31): LASER/BLEEP/ZAP/GLITCH");
  Serial.println();
  Serial.println("OLED Pages:");
  Serial.println("  LIVE, TONE, ENV, LOFI, REC, SCOPE");
  Serial.println("  SCOPE = Live waveform visualization");
  Serial.println();
  Serial.println("Effects (encoder button to cycle):");
  Serial.println("  OFF = Clean, no effects");
  Serial.println("  BITCRUSH = 6-bit degradation");
  Serial.println("  DISTORT = XOR + wavefolding");
  Serial.println("  DELAY = Sample rate reduction (lo-fi)");
  Serial.println("  FM = Frequency modulation (metallic/bell tones)");
  Serial.println("  LFO = Low-freq oscillator (vibrato/tremolo)");
  Serial.println();
  Serial.println("Recording:");
  Serial.println("  Press D10 to start/stop recording");
  Serial.println("  Files saved as /sample000.wav, /sample001.wav, etc.");
  Serial.println("  Recordings are 16-bit 44.1kHz WAV (CD quality)");
  Serial.println("  OLED page 5 (REC) shows recording status");
  Serial.println();
  Serial.flush();
}

int loopCounter = 0;

// ---------------------------------------------------------------------------
void loop() {
  // ---- POTS: Read analog inputs and control synth ----
  float rawCutoff = analogRead(POT_CUTOFF) / 4095.0f;
  float rawDrive  = analogRead(POT_DRIVE)  / 4095.0f;
  float rawDecay  = analogRead(POT_DECAY)  / 4095.0f;
  float rawPitch  = analogRead(POT_PITCH)  / 4095.0f;

  // Exponential smoothing to reduce noise
  smoothCutoff += POT_ALPHA * (rawCutoff - smoothCutoff);
  smoothDrive  += POT_ALPHA * (rawDrive  - smoothDrive);
  smoothDecay  += POT_ALPHA * (rawDecay  - smoothDecay);
  smoothPitch  += POT_ALPHA * (rawPitch  - smoothPitch);

  // Map to synth parameters
  float decay  = 0.02f + smoothDecay * 2.0f;           // 20ms to 2 seconds
  float pitch  = 55.0f * powf(2.0f, smoothPitch * 4.0f); // 55Hz to 880Hz (4 octaves)

  // Apply pot values continuously (so they override presets and respond in real-time)
  voice.setCutoff(smoothCutoff);
  voice.setDrive(smoothDrive);
  voice.setDecay(decay);
  // pitch is set during note trigger and while held

  display.cutoffShown = smoothCutoff;
  display.driveShown  = smoothDrive;
  display.decayShown  = smoothDecay;
  display.resShown    = 0.0f;
  display.pitchShown  = pitch;
  display.waveformNameShown = waveformNames[currentWaveform];
  display.effectNameShown = effectNames[currentEffect];

  // ---- read buttons with debouncing ----
  bool trig = (digitalRead(BTN_TRIG) == LOW);
  bool sound = (digitalRead(BTN_SOUND) == LOW);
  bool rec = (digitalRead(BTN_REC) == LOW);

  // ---- normal button operations ----
  display.trigShown = trig;

  // Trigger button - with debouncing
  if (trig && !lastTrig && trigDebounce > DEBOUNCE_MS) {
    trigDebounce = 0;
    Serial.print(">>> NOTE ON - Pitch: ");
    Serial.print((int)pitch);
    Serial.println(" Hz");
    AudioNoInterrupts();
    voice.setPitch(pitch);
    voice.noteOn();
    AudioInterrupts();
  }
  if (!trig && lastTrig && trigDebounce > DEBOUNCE_MS) {
    trigDebounce = 0;
    Serial.println(">>> NOTE OFF");
    voice.noteOff();
  }

  // Update pitch continuously while note is held (allows pots to affect sound)
  if (trig && voice.active()) {
    voice.setPitch(pitch);
  }

  // D9 button - cycle sounds/presets
  if (sound && !lastSound && soundDebounce > DEBOUNCE_MS) {
    soundDebounce = 0;
    currentWaveform = (currentWaveform + 1) % NUM_WAVEFORMS;
    applySoundPreset(currentWaveform);
    Serial.print(">>> SOUND CHANGED: ");
    Serial.println(waveformNames[currentWaveform]);
  }

  // D10 button - toggle recording
  if (rec && !lastRec && recDebounce > DEBOUNCE_MS) {
    recDebounce = 0;
    Serial.print("D10 pressed! Recording: ");
    Serial.println(wav.isRecording() ? "YES" : "NO");
    if (wav.isRecording()) {
      Serial.println("Stopping recording...");
      stopRecording();
    } else {
      Serial.println("Starting recording...");
      startRecording();
    }
  }

  lastTrig = trig;
  lastSound = sound;
  lastRec = rec;

  // ---- encoder: rotation = page select, button = effect toggle ----
  int8_t step = pollEncoderStep();
  if (step != 0) display.nextPage();

  bool encSw = (digitalRead(ENC_SW) == LOW);

  // Encoder button press - cycle effects
  if (encSw && !lastEncSw) {
    // Button pressed - toggle to next effect
    currentEffect = (currentEffect + 1) % 6;  // 6 effects (OFF/BITCRUSH/DISTORT/DELAY/FM/LFO)

    // First, reset all effects to clean state
    voice.setBitDepth(16.0f);
    voice.setXorAmount(0.0f);
    voice.setFold(0.0f);
    voice.setSampleRateReduce(0.0f);
    voice.setFM(false);
    voice.setLFO(false);

    // Apply effect settings
    switch (currentEffect) {
      case 0:  // OFF - all effects disabled
        Serial.println(">>> EFFECTS: OFF");
        break;

      case 1:  // BITCRUSH
        voice.setBitDepth(6.0f);   // Crunchy 6-bit
        Serial.println(">>> EFFECTS: BITCRUSH (6-bit)");
        break;

      case 2:  // DISTORTION
        voice.setXorAmount(0.4f);  // XOR distortion
        voice.setFold(0.3f);       // Wavefolding
        Serial.println(">>> EFFECTS: DISTORTION (XOR+Fold)");
        break;

      case 3:  // DELAY (using sample rate reduction for lo-fi delay effect)
        voice.setSampleRateReduce(0.5f);  // Sample rate reduce for texture
        Serial.println(">>> EFFECTS: DELAY/LOFI (sample reduce)");
        break;

      case 4:  // FM - Frequency Modulation
        voice.setFM(true);
        voice.setFMRatio(2.0f);    // 2x harmonic
        voice.setFMDepth(0.3f);    // Moderate depth
        Serial.println(">>> EFFECTS: FM (freq modulation, ratio=2.0, depth=0.3)");
        break;

      case 5:  // LFO - Low Frequency Oscillator (vibrato)
        voice.setLFO(true);
        voice.setLFORate(5.0f);    // 5 Hz
        voice.setLFODepth(0.05f);  // Subtle vibrato
        Serial.println(">>> EFFECTS: LFO (vibrato, 5Hz, depth=0.05)");
        break;
    }
  }
  lastEncSw = encSw;

  // ---- Record queue draining (only when recording to avoid accessing inactive queue) ----
  if (wav.isRecording()) {
    wav.update();
    wav.update();
    wav.update();
  }

  // ---- Update scope visualization (reduced rate to prevent loop blocking) ----
  if (scopeUpdate > 10) {  // Update every 10ms (was 1ms - too aggressive)
    scopeUpdate = 0;

    // Generate simulated waveform based on current settings
    static float scopePhase = 0.0f;
    static int lastWaveform = -1;
    float sample = 0.0f;

    // Reset phase when waveform changes to prevent glitchy transitions
    if (currentWaveform != lastWaveform) {
      scopePhase = 0.0f;
      lastWaveform = currentWaveform;
    }

    if (voice.active()) {
      // Generate waveform based on type (show actual waveform shape)
      int waveType = currentWaveform;

      // For drum presets (6-19), show the base waveform they use
      if (waveType >= 6 && waveType <= 19) {
        if (waveType == 6 || waveType == 10) waveType = 1;  // KICK, TOM = SINE
        else if (waveType == 7 || waveType == 8 || waveType == 9 || waveType == 17 || waveType == 19) waveType = 4;  // SNARE, HIHAT, CLAP, CYMBAL, SHAKER = NOISE
        else if (waveType == 11 || waveType == 16) waveType = 2;  // COWBELL, RIMSHOT = SQUARE
        else if (waveType == 18) waveType = 3;  // WOODBLOCK = TRI
      }

      // For synth presets (20-27), show their base waveforms
      if (waveType >= 20 && waveType <= 27) {
        if (waveType == 20) waveType = 5;  // PLUCK = KARPLUS
        else if (waveType == 21 || waveType == 26) waveType = 1;  // ORGAN, RHODES = SINE
        else if (waveType == 22 || waveType == 23 || waveType == 27) waveType = 0;  // BRASS, STRINGS, WOBBLE = SAW
        else if (waveType == 24 || waveType == 29) waveType = 2;  // ARP, BLEEP = SQUARE
        else if (waveType == 25) waveType = 3;  // CHOIR = TRI
      }

      // For FX presets (28-31), show their base waveforms
      if (waveType >= 28 && waveType <= 31) {
        if (waveType == 28) waveType = 0;  // LASER = SAW
        else if (waveType == 29 || waveType == 31) waveType = 2;  // BLEEP, GLITCH = SQUARE
        else if (waveType == 30) waveType = 4;  // ZAP = NOISE
      }

      // Generate actual waveform
      switch (waveType) {
        case 0:  // SAW
          sample = 2.0f * scopePhase - 1.0f;
          break;
        case 1:  // SINE
          sample = sinf(scopePhase * 6.28318f);
          break;
        case 2:  // SQUARE
          sample = (scopePhase < 0.5f) ? 1.0f : -1.0f;
          break;
        case 3:  // TRI
          sample = 1.0f - 4.0f * fabsf(scopePhase - 0.5f);
          break;
        case 4:  // NOISE
          sample = (random(2000) - 1000) / 1000.0f;
          break;
        case 5:  // KARPLUS (simulated decay)
          sample = sinf(scopePhase * 6.28318f) * (1.0f - scopePhase * 0.5f);
          break;
      }

      // Advance phase MUCH faster for accurate representation
      scopePhase += 0.15f;  // Faster advancement = tighter waveform display
      if (scopePhase >= 1.0f) scopePhase -= 1.0f;
    } else {
      sample = 0.0f;  // Silent when not active
      scopePhase = 0.0f;  // Reset when inactive
    }

    display.updateScope(sample);
  }

  // ---- UI refresh at ~10fps (reduced from 20fps to prevent I2C blocking) ----
  if (uiRefresh > 100) {
    uiRefresh = 0;
    // Pass recording status and free space to display
    display.draw(voice, wav.isRecording(), wav.secondsWritten(), sdFreeMB());
  }

  // ---- pot debug @ 0.2Hz: check if pots are working (reduced to prevent Serial blocking) ----
  if (debugReport > 5000) {
    debugReport = 0;

    // Read raw ADC values
    int rawA0 = analogRead(POT_CUTOFF);
    int rawA1 = analogRead(POT_DRIVE);
    int rawA2 = analogRead(POT_DECAY);
    int rawA3 = analogRead(POT_PITCH);

    // Show detailed diagnosis
    Serial.println("===== POT DIAGNOSIS =====");
    Serial.print("RAW ADC: A0=");
    Serial.print(rawA0);
    Serial.print(" A1=");
    Serial.print(rawA1);
    Serial.print(" A2=");
    Serial.print(rawA2);
    Serial.print(" A3=");
    Serial.println(rawA3);

    Serial.print("NORMALIZED (0-1): Cut=");
    Serial.print(rawA0 / 4095.0f, 3);
    Serial.print(" Drv=");
    Serial.print(rawA1 / 4095.0f, 3);
    Serial.print(" Dec=");
    Serial.print(rawA2 / 4095.0f, 3);
    Serial.print(" Pitch=");
    Serial.println(rawA3 / 4095.0f, 3);

    Serial.print("SMOOTHED: Cut=");
    Serial.print(smoothCutoff, 3);
    Serial.print(" Drv=");
    Serial.print(smoothDrive, 3);
    Serial.print(" Dec=");
    Serial.print(smoothDecay, 3);
    Serial.print(" Pitch=");
    Serial.println(smoothPitch, 3);

    Serial.print("APPLIED: Decay=");
    Serial.print(decay, 3);
    Serial.print("s Pitch=");
    Serial.print((int)pitch);
    Serial.println("Hz");

    Serial.print("SOUND: ");
    Serial.print(waveformNames[currentWaveform]);
    Serial.print(" | FX: ");
    Serial.println(effectNames[currentEffect]);
    Serial.println("=========================");
    Serial.flush();  // Prevent Serial buffer blocking
  }

  // ---- audio engine health check @ 5 seconds (reduced to prevent Serial blocking) ----
  if (cpuReport > 5000) {
    cpuReport = 0;
    float cpu = AudioProcessorUsage();
    float cpuMax = AudioProcessorUsageMax();
    int mem = AudioMemoryUsageMax();

    Serial.print("AUDIO: CPU=");
    Serial.print(cpu, 1);
    Serial.print("% (max:");
    Serial.print(cpuMax, 1);
    Serial.print("%) Mem=");
    Serial.print(mem);
    Serial.print("/70");

    if (voice.active()) {
      Serial.print(" [PLAYING]");
    } else {
      Serial.print(" [IDLE]");
    }

    // Warning if issues detected
    if (cpu > 80.0) {
      Serial.print(" *** CPU OVERLOAD! ***");
    }
    if (cpuMax > 95.0) {
      Serial.print(" *** CPU SPIKE DETECTED! ***");
    }
    if (mem > 55) {
      Serial.print(" *** MEM HIGH! ***");
    }
    if (mem >= 65) {
      Serial.print(" *** MEM CRITICAL - AUDIO WILL FAIL! ***");
    }
    Serial.println();
    Serial.flush();  // Prevent Serial buffer blocking

    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
  }

  // ---- loop() performance monitoring ----
  // Check if loop() is getting blocked (should run <1ms ideally)
  static unsigned long maxLoopTime = 0;
  unsigned long loopTime = loopTimer;
  if (loopTime > maxLoopTime) {
    maxLoopTime = loopTime;
    if (loopTime > 10000) {  // >10ms is concerning
      Serial.print("*** SLOW LOOP: ");
      Serial.print(loopTime);
      Serial.println(" us - this can cause audio dropouts! ***");
    }
  }
  loopTimer = 0;
}
