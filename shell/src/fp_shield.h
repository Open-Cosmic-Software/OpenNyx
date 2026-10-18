// Copyright (c) 2026 Open Cosmic Software. MIT licensed.

#ifndef OPENNYX_SHELL_SRC_FP_SHIELD_H_
#define OPENNYX_SHELL_SRC_FP_SHIELD_H_

#include <string>

// Fingerprint shield (M4.1): a script that runs in every web page's V8
// context BEFORE the page's own scripts (OnContextCreated, render process).
//
// It reduces the usual JS fingerprinting surface:
//  * canvas: toDataURL / toBlob / getImageData get a few flipped low bits
//  * WebGL: common vendor/renderer string, readPixels noise
//  * audio: inaudible noise (~1e-7) in AudioBuffer / AnalyserNode reads
//  * navigator.hardwareConcurrency / deviceMemory normalised to 8
//
// The noise is deterministic per (browser session, site): stable while you
// browse a site (so it does not break), different on every other site and
// after every restart (so you cannot be linked across them).
//
// Honest limits (also in docs/FINGERPRINT-SHIELD.md): JS-level, so it is
// detectable, it does not reach Web Workers, and the font list, screen size
// and time zone still leak. A real fix needs engine patches (Approach A in
// docs/ENGINE-STRATEGY.md).

// Command-line switch the browser process passes to every renderer. Absent =
// shield off.
extern const char kFingerprintSeedSwitch[];

// Returns a fresh random per-session seed (hex).
std::string NewFingerprintSeed();

// Returns the full script for |seed| (seed is hex only, no escaping needed).
std::string BuildFingerprintShield(const std::string& seed);

#endif  // OPENNYX_SHELL_SRC_FP_SHIELD_H_
