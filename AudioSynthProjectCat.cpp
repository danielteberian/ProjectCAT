/*
 * AudioSynthProjectCat.cpp
 * ------------------------
 * DSP implementation for the Project Cat Teensy voice. See header for chain.
 */

#include "AudioSynthProjectCat.h"
#include <math.h>

static const float PI_F     = 3.14159265358979f;
static const float TWO_PI_F = 6.28318530717959f;

static inline int16_t clamp16(float v) {
  if (v >  32767.0f) return  32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)v;
}

// ---------------------------------------------------------------------------
// Construction: AudioStream(0 inputs, no input queue). Diagnostic baseline
// patch -- FM / LFO / sustain OFF (all were `true` on the ESP32 build and
// produced a masking drone). Restore once a clean signal is confirmed.
// ---------------------------------------------------------------------------
AudioSynthProjectCat::AudioSynthProjectCat() : AudioStream(0, NULL) {
  pitchHz    = 220.0f;
  cutoffNorm = 0.70f;
  resonance  = 0.20f;
  drive      = 0.30f;
  attackSec  = 0.005f;
  decaySec   = 0.40f;
  outGain    = 0.80f;

  srrAmt     = 0.0f;
  bitDepth   = 16.0f;
  xorAmt     = 0.0f;
  foldAmt    = 0.0f;

  fmRatio    = 2.0f;
  fmDepth    = 0.5f;
  lfoHz      = 5.0f;
  lfoDepth   = 0.0f;

  waveform   = WAVE_SAW;
  fmOn       = false;   // was true
  lfoOn      = false;   // was true
  sustainMode= false;   // was true
  gate       = false;

  attackInc  = 0.0f;
  decayCoef  = 0.0f;
  svf_g = svf_k = svf_a1 = svf_a2 = svf_a3 = 0.0f;

  phase = fmPhase = lfoPhase = 0.0f;
  srPhase = 0.0f; srHold = 0.0f;
  env = 0.0f; envStage = ENV_IDLE;
  ic1eq = ic2eq = 0.0f;
  res_z1 = res_z2 = 0.0f;
  rngState = 0x1234567u;
  for (int i = 0; i < KS_MAX; i++) ksBuf[i] = 0.0f;
  ksLen = 0; ksIdx = 0; ksDamp = 0.996f;
}

// ---------------------------------------------------------------------------
// Note control
// ---------------------------------------------------------------------------
void AudioSynthProjectCat::noteOn() {
  if (waveform == WAVE_KS) pluckKS(pitchHz);
  gate     = true;
  envStage = ENV_ATTACK;
}
void AudioSynthProjectCat::noteOn(float freqHz) { setPitch(freqHz); noteOn(); }

void AudioSynthProjectCat::noteOff() {
  gate = false;
  if (envStage == ENV_SUSTAIN) envStage = ENV_RELEASE;  // ASR release
  // In AD mode the decay already runs to zero; nothing to force.
}

// ---------------------------------------------------------------------------
// Sources
// ---------------------------------------------------------------------------
float AudioSynthProjectCat::whiteNoise() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return (float)((int32_t)rngState) / 2147483648.0f;
}

float AudioSynthProjectCat::osc(float p) {
  switch (waveform) {
    case WAVE_SINE:   return sinf(TWO_PI_F * p);
    case WAVE_SAW:    return 2.0f * p - 1.0f;
    case WAVE_SQUARE: return (p < 0.5f) ? 1.0f : -1.0f;
    case WAVE_TRI:    return 1.0f - 4.0f * fabsf(p - 0.5f);
    case WAVE_NOISE:  return whiteNoise();
    default:          return 0.0f;
  }
}

void AudioSynthProjectCat::pluckKS(float f) {
  ksLen = (int)(AUDIO_SAMPLE_RATE_EXACT / f);
  if (ksLen < 2)      ksLen = 2;
  if (ksLen > KS_MAX) ksLen = KS_MAX;
  for (int n = 0; n < ksLen; n++) ksBuf[n] = whiteNoise();
  ksIdx = 0;
}

float AudioSynthProjectCat::renderKS() {
  if (ksLen < 2) return 0.0f;
  int j = ksIdx + 1; if (j >= ksLen) j = 0;
  float y = 0.5f * (ksBuf[ksIdx] + ksBuf[j]);   // averaging lowpass
  ksBuf[ksIdx] = y * ksDamp;                    // feedback with damping
  ksIdx = j;
  return y;
}

// ---------------------------------------------------------------------------
// Per-sample render: the full chain
// ---------------------------------------------------------------------------
float AudioSynthProjectCat::renderSample(float fs) {
  // ----- modulation sources -----
  float lfo = 0.0f;
  if (lfoOn) {
    lfoPhase += lfoHz / fs;
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    lfo = sinf(TWO_PI_F * lfoPhase) * lfoDepth;
  }
  float f = pitchHz * (1.0f + lfo);
  if (f < 8.0f) f = 8.0f;

  float fmMod = 0.0f;
  if (fmOn) {
    fmPhase += (f * fmRatio) / fs;
    if (fmPhase >= 1.0f) fmPhase -= 1.0f;
    fmMod = sinf(TWO_PI_F * fmPhase) * fmDepth;
  }

  // ----- source -----
  float src;
  if (waveform == WAVE_KS) {
    src = renderKS();
  } else {
    phase += (f / fs) * (1.0f + fmMod);   // phase-modulation FM
    while (phase >= 1.0f) phase -= 1.0f;
    while (phase <  0.0f) phase += 1.0f;
    src = osc(phase);
  }

  // ----- sample-rate reducer (sample & hold decimation) -----
  float maxHold = 1.0f + srrAmt * 60.0f;
  srPhase += 1.0f;
  if (srPhase >= maxHold) { srPhase -= maxHold; srHold = src; }
  float x = srHold;

  // ----- bitcrusher -----
  if (bitDepth < 15.9f) {
    float levels = powf(2.0f, bitDepth);
    float step   = 2.0f / levels;
    x = step * floorf(x / step + 0.5f);
  }

  // ----- XOR distortion (the grit) -----
  if (xorAmt > 0.0001f) {
    int16_t xi   = (int16_t)(x * 32767.0f);
    int16_t mask = (int16_t)(xorAmt * 4095.0f);
    xi ^= mask;
    x = (float)xi / 32767.0f;
  }

  // ----- wavefolder (reflective) -----
  if (foldAmt > 0.0001f) {
    float g  = 1.0f + foldAmt * 4.0f;
    float xf = x * g;
    for (int n = 0; n < 6; n++) {
      if      (xf >  1.0f) xf =  2.0f - xf;
      else if (xf < -1.0f) xf = -2.0f - xf;
      else break;
    }
    x = xf;
  }

  // ----- tanh waveshaper (drive) -----
  x = tanhf(x * (1.0f + drive * 15.0f));

  // ----- resonator (bounded 2-pole, tuned to pitch) -----
  {
    float w0 = TWO_PI_F * f / fs;
    if (w0 > 3.0f) w0 = 3.0f;
    float r  = 0.80f + resonance * 0.18f;       // 0.80..0.98 (stable)
    float a1 = 2.0f * r * cosf(w0);
    float a2 = -r * r;
    float y  = (1.0f - r) * x + a1 * res_z1 + a2 * res_z2;
    res_z2 = res_z1; res_z1 = y;
    x = y;
  }

  // ----- TPT state-variable lowpass (Cytomic) -----
  float v3 = x - ic2eq;
  float v1 = svf_a1 * ic1eq + svf_a2 * v3;
  float v2 = ic2eq + svf_a2 * ic1eq + svf_a3 * v3;
  ic1eq = 2.0f * v1 - ic1eq;
  ic2eq = 2.0f * v2 - ic2eq;
  x = v2;                                        // lowpass tap

  // ----- envelope -----
  switch (envStage) {
    case ENV_ATTACK:
      env += attackInc;
      if (env >= 1.0f) { env = 1.0f; envStage = sustainMode ? ENV_SUSTAIN : ENV_DECAY; }
      break;
    case ENV_DECAY:
      env *= decayCoef;
      if (env < 0.0003f) { env = 0.0f; envStage = ENV_IDLE; }
      break;
    case ENV_SUSTAIN:
      break;                                     // held until noteOff()
    case ENV_RELEASE:
      env *= decayCoef;
      if (env < 0.0003f) { env = 0.0f; envStage = ENV_IDLE; }
      break;
    case ENV_IDLE:
    default:
      env = 0.0f;
      break;
  }
  x *= env;

  return x * outGain;
}

// ---------------------------------------------------------------------------
// AudioStream callback: per-block coefficient update + fill 128 samples
// ---------------------------------------------------------------------------
void AudioSynthProjectCat::update(void) {
  audio_block_t *block = allocate();
  if (!block) return;

  const float fs = AUDIO_SAMPLE_RATE_EXACT;

  // --- per-block coefficients (cheap, once per 128 samples) ---
  attackInc = 1.0f / (attackSec * fs);
  decayCoef = expf(-1.0f / (decaySec * fs));     // exp decay / release

  float fc = 20.0f * powf(900.0f, cutoffNorm);   // 20 Hz .. ~18 kHz, log
  if (fc > fs * 0.45f) fc = fs * 0.45f;
  svf_g  = tanf(PI_F * fc / fs);
  svf_k  = 2.0f - 1.96f * resonance;             // res 0..0.98 -> k 2..0.08
  svf_a1 = 1.0f / (1.0f + svf_g * (svf_g + svf_k));
  svf_a2 = svf_g * svf_a1;
  svf_a3 = svf_g * svf_a2;

  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
    block->data[i] = clamp16(renderSample(fs) * 32767.0f);
  }

  transmit(block, 0);
  release(block);
}
