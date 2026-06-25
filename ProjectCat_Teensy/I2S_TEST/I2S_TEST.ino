/*
 * I2S_TEST.ino
 * Simple I2S audio test for Teensy 4.1 + PCM5102
 * Should output a continuous 440Hz sine wave tone
 * If you hear this, I2S is working!
 */

#include <Audio.h>

// Create simple test tone
AudioSynthWaveformSine sineWave;
AudioOutputI2S         i2sOut;
AudioConnection        patchCord(sineWave, 0, i2sOut, 0);
AudioConnection        patchCordR(sineWave, 0, i2sOut, 1);

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("========================================");
  Serial.println("I2S AUDIO TEST - Teensy 4.1 + PCM5102");
  Serial.println("========================================");
  Serial.println();
  Serial.println("This will output a continuous 440Hz tone.");
  Serial.println();
  Serial.println("If you hear it:");
  Serial.println("  ✓ I2S is working");
  Serial.println("  ✓ PCM5102 DAC is working");
  Serial.println("  ✓ LM386 amp is working");
  Serial.println("  ✓ Speaker is working");
  Serial.println("  → Problem is in main synth code");
  Serial.println();
  Serial.println("If you DON'T hear it:");
  Serial.println("  ✗ Check I2S wiring:");
  Serial.println("    Pin 21 → PCM5102 BCLK");
  Serial.println("    Pin 20 → PCM5102 LRCLK");
  Serial.println("    Pin 7  → PCM5102 DIN");
  Serial.println("  ✗ Check PCM5102 XSMT → 3.3V (unmute)");
  Serial.println("  ✗ Check PCM5102 SCK → GND (slave mode)");
  Serial.println("  ✗ Check power to PCM5102 (VIN → 3.3V)");
  Serial.println();
  Serial.println("========================================");
  Serial.println("Starting tone in 2 seconds...");
  Serial.println("========================================");

  delay(2000);

  // Initialize audio
  AudioMemory(10);

  // Start 440Hz sine wave at 50% volume
  sineWave.frequency(440);
  sineWave.amplitude(0.5);

  Serial.println();
  Serial.println(">>> TONE STARTED <<<");
  Serial.println("You should hear a continuous 440Hz tone now.");
  Serial.println();
  Serial.println("Audio system stats will print every 2 seconds:");
}

void loop() {
  static unsigned long lastReport = 0;

  if (millis() - lastReport > 2000) {
    lastReport = millis();

    float cpu = AudioProcessorUsage();
    float cpuMax = AudioProcessorUsageMax();
    int mem = AudioMemoryUsageMax();

    Serial.print("CPU: ");
    Serial.print(cpu, 1);
    Serial.print("% (max: ");
    Serial.print(cpuMax, 1);
    Serial.print("%)  |  Memory: ");
    Serial.print(mem);
    Serial.print("/10 blocks  |  ");

    if (cpu < 10.0 && mem < 5) {
      Serial.println("Status: OK");
    } else if (cpu > 50.0 || mem > 8) {
      Serial.println("Status: WARNING - high usage!");
    } else {
      Serial.println("Status: GOOD");
    }

    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
  }
}
