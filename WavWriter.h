/*
 * WavWriter.h
 * -----------
 * Minimal mono 16-bit/44.1kHz WAV recorder for the Teensy Audio Library.
 *
 * Usage:
 *   WavWriter rec;
 *   rec.begin("/sample001.wav");   // opens file, writes placeholder header
 *   ... in loop(): rec.update();   // drains the AudioRecordQueue to SD
 *   rec.end();                     // patches header with real sizes, closes
 *
 * Pattern: AudioRecordQueue buffers 128-sample (256-byte) blocks from the
 * audio graph. loop() polls queue.available() and appends each block
 * directly to the open file as raw PCM. On end(), seek back to byte 0 and
 * rewrite the 44-byte RIFF/WAVE header now that the final size is known.
 */

#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include <Arduino.h>
#include <SD.h>
#include <Audio.h>

class WavWriter {
public:
  WavWriter(AudioRecordQueue &q) : queue(q), file(), recording(false), dataBytes(0) {}

  bool begin(const char *path) {
    if (SD.exists(path)) SD.remove(path);
    file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    writeHeaderPlaceholder();
    dataBytes = 0;
    // Note: queue.begin() is now called in setup(), not here
    recording = true;
    return true;
  }

  // Call every loop() iteration - MUST drain queue even when not recording!
  void update() {
    if (!recording) {
      // CRITICAL: Drain and discard blocks when not recording
      // Otherwise the queue fills up and blocks the audio stream!
      while (queue.available() >= 1) {
        queue.readBuffer();  // Get buffer
        queue.freeBuffer();  // Immediately free without writing
      }
      return;
    }

    // When recording, drain and write to file
    while (queue.available() >= 1) {
      uint8_t *block = (uint8_t *)queue.readBuffer();
      file.write(block, 256);     // 128 samples * 2 bytes
      dataBytes += 256;
      queue.freeBuffer();
    }
  }

  void end() {
    if (!recording) return;

    // Drain any remaining queued blocks before closing
    while (queue.available() > 0) {
      uint8_t *block = (uint8_t *)queue.readBuffer();
      file.write(block, 256);
      dataBytes += 256;
      queue.freeBuffer();
    }

    patchHeader();
    file.close();
    recording = false;
    // Note: Don't call queue.end() - queue stays running for continuous drain
  }

  bool isRecording() const { return recording; }
  uint32_t bytesWritten() const { return dataBytes; }
  float secondsWritten() const { return dataBytes / (2.0f * SAMPLE_RATE); }

private:
  static const uint32_t SAMPLE_RATE = 44100;
  static const uint16_t NUM_CHANNELS = 1;
  static const uint16_t BITS_PER_SAMPLE = 16;

  AudioRecordQueue &queue;
  File file;
  bool recording;
  uint32_t dataBytes;

  void writeU32(uint32_t v) { file.write((uint8_t *)&v, 4); }
  void writeU16(uint16_t v) { file.write((uint8_t *)&v, 2); }

  // Write a 44-byte canonical PCM WAV header with sizes = 0 (patched later).
  void writeHeaderPlaceholder() {
    file.write("RIFF", 4);
    writeU32(0);                                   // ChunkSize (patched)
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    writeU32(16);                                   // Subchunk1Size (PCM)
    writeU16(1);                                    // AudioFormat = PCM
    writeU16(NUM_CHANNELS);
    writeU32(SAMPLE_RATE);
    uint32_t byteRate = SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    writeU32(byteRate);
    uint16_t blockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    writeU16(blockAlign);
    writeU16(BITS_PER_SAMPLE);
    file.write("data", 4);
    writeU32(0);                                   // Subchunk2Size (patched)
  }

  void patchHeader() {
    uint32_t chunkSize = 36 + dataBytes;
    file.seek(4);
    writeU32(chunkSize);
    file.seek(40);
    writeU32(dataBytes);
    file.flush();
  }
};

#endif // WAV_WRITER_H
