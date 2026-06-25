/*
 * AudioSynthProjectCat.h
 * ----------------------
 * Project Cat synth voice ported to the Teensy 4.1 Audio Library.
 *
 * SOURCE node: 0 inputs, 1 output. Runs the full Project Cat signal chain
 * inside AudioStream::update(), filling 128-sample blocks at
 * AUDIO_SAMPLE_RATE_EXACT (44100 Hz), DMA/interrupt-driven. Replaces the
 * ESP32-S3 core-0 FreeRTOS audio task entirely.
 *
 * Signal chain (unchanged from the ESP32 build):
 *   source (OSC / Karplus-Strong) [+ FM, + LFO]
 *     -> sample-rate reducer
 *     -> bitcrusher
 *     -> XOR distortion
 *     -> wavefolder
 *     -> tanh waveshaper (drive)
 *     -> resonator (bounded 2-pole)
 *     -> TPT state-variable lowpass (cutoff / resonance)
 *     -> AD / ASR envelope
 *     -> output gain
 *     -> [AudioOutputI2S downstream]
 *
 * Concurrency: parameters are plain 32-bit / bool fields written from loop()
 * and read in update() (the audio ISR). Single aligned writes are atomic on
 * the Cortex-M7, so individual setters are safe to call directly. For
 * multi-field updates that must stay consistent (e.g. a note trigger), wrap
 * the calls in AudioNoInterrupts()/AudioInterrupts() in the caller.
 */

#ifndef AUDIO_SYNTH_PROJECT_CAT_H
#define AUDIO_SYNTH_PROJECT_CAT_H

#include <Arduino.h>
#include <AudioStream.h>

class AudioSynthProjectCat : public AudioStream {
public:
  enum Waveform : uint8_t {
    WAVE_SINE = 0, WAVE_SAW, WAVE_SQUARE, WAVE_TRI, WAVE_NOISE, WAVE_KS
  };

  AudioSynthProjectCat();
  virtual void update(void);

  // ---- note control ----
  void noteOn(float freqHz);
  void noteOn();                 // uses current pitch
  void noteOff();

  // ---- core params (each safe to call individually from loop()) ----
  void setPitch(float hz)        { pitchHz   = constrain(hz, 8.0f, 8000.0f); }
  void setWaveform(Waveform w)   { waveform  = w; }
  void setCutoff(float norm)     { cutoffNorm= constrain(norm, 0.0f, 1.0f); }
  void setResonance(float r)     { resonance = constrain(r, 0.0f, 0.98f); }
  void setDrive(float d)         { drive     = constrain(d, 0.0f, 1.0f); }
  void setAttack(float sec)      { attackSec = constrain(sec, 0.0005f, 2.0f); }
  void setDecay(float sec)       { decaySec  = constrain(sec, 0.005f, 8.0f); }
  void setOutputGain(float g)    { outGain   = constrain(g, 0.0f, 1.0f); }

  // ---- lo-fi character ----
  void setSampleRateReduce(float n){ srrAmt   = constrain(n, 0.0f, 1.0f); }
  void setBitDepth(float bits)   { bitDepth = constrain(bits, 1.0f, 16.0f); }
  void setXorAmount(float x)     { xorAmt   = constrain(x, 0.0f, 1.0f); }
  void setFold(float f)          { foldAmt  = constrain(f, 0.0f, 1.0f); }

  // ---- modulation (default OFF -- were true on ESP32) ----
  void setFM(bool on)            { fmOn      = on; }
  void setFMRatio(float r)       { fmRatio   = r; }
  void setFMDepth(float d)       { fmDepth   = d; }
  void setLFO(bool on)           { lfoOn     = on; }
  void setLFORate(float hz)      { lfoHz     = hz; }
  void setLFODepth(float d)      { lfoDepth  = d; }
  void setSustainMode(bool on)   { sustainMode = on; }

  bool active() const            { return envStage != ENV_IDLE; }

private:
  enum EnvStage : uint8_t { ENV_IDLE = 0, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
  static const int KS_MAX = 2048;   // lowest note ~21.5 Hz @ 44.1k

  float renderSample(float fs);
  float osc(float p);
  float whiteNoise();
  float renderKS();
  void  pluckKS(float f);

  // ---- parameters ----
  float pitchHz;
  float cutoffNorm;
  float resonance;
  float drive;
  float attackSec;
  float decaySec;
  float outGain;
  float srrAmt;
  float bitDepth;
  float xorAmt;
  float foldAmt;
  float fmRatio, fmDepth;
  float lfoHz, lfoDepth;
  Waveform waveform;
  bool fmOn, lfoOn, sustainMode;
  bool gate;

  // ---- per-block precomputed coefficients ----
  float attackInc;
  float decayCoef;
  float svf_g, svf_k, svf_a1, svf_a2, svf_a3;

  // ---- per-sample state ----
  float phase, fmPhase, lfoPhase;
  float srPhase, srHold;
  float env;
  EnvStage envStage;
  float ic1eq, ic2eq;          // SVF
  float res_z1, res_z2;        // resonator
  uint32_t rngState;
  float ksBuf[KS_MAX];
  int   ksLen, ksIdx;
  float ksDamp;
};

#endif // AUDIO_SYNTH_PROJECT_CAT_H
