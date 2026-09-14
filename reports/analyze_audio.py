"""Measure mono, 16-bit PCM WAV files using a whole-file Hann-window FFT."""

import json
import sys
import wave

import numpy as np


def measure(path):
    with wave.open(str(path), "rb") as source:
        fs = source.getframerate()
        frames = source.getnframes()
        if source.getnchannels() != 1 or source.getsampwidth() != 2 or frames < 3:
            raise ValueError("expected mono 16-bit PCM with at least 3 frames")
        samples = np.frombuffer(source.readframes(frames), dtype="<i2").astype(float) / 32768
    spectrum = np.abs(np.fft.rfft(samples * np.hanning(frames)))
    k = int(np.argmax(spectrum[1:]) + 1)
    return {
        "file": str(path),
        "sample_rate_hz": fs,
        "frames": frames,
        "duration_s": frames / fs,
        "peak_frequency_hz": k * fs / frames if np.any(samples) else None,
        "rms": float(np.sqrt(np.mean(samples ** 2))),
        "peak": float(np.max(np.abs(samples))),
    }


if __name__ == "__main__":
    print(json.dumps([measure(path) for path in sys.argv[1:]], indent=2))
