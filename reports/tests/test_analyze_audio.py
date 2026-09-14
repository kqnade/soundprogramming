import math
from pathlib import Path
import struct
import tempfile
import unittest
import wave
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from analyze_audio import measure


class MeasurementTests(unittest.TestCase):
    def test_one_second_440_hz_tone(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tone.wav"
            with wave.open(str(path), "wb") as output:
                output.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
                output.writeframes(b"".join(struct.pack("<h", round(
                    16384 * math.sin(2 * math.pi * 440 * n / 48000)
                )) for n in range(48000)))
            result = measure(path)
        self.assertEqual(result["frames"], 48000)
        self.assertEqual(result["duration_s"], 1.0)
        self.assertAlmostEqual(result["peak_frequency_hz"], 440, delta=0.1)
        self.assertAlmostEqual(result["rms"], 0.5 / math.sqrt(2), delta=0.0001)
        self.assertAlmostEqual(result["peak"], 0.5, delta=0.0001)

    def test_silence_has_no_peak_frequency(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "silence.wav"
            with wave.open(str(path), "wb") as output:
                output.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
                output.writeframes(bytes(96000))
            result = measure(path)
        self.assertIsNone(result["peak_frequency_hz"])
        self.assertEqual(result["rms"], 0.0)
        self.assertEqual(result["peak"], 0.0)

    def test_rejects_unsupported_or_too_short_pcm(self):
        for channels, width, frames in [(2, 2, 100), (1, 3, 100), (1, 2, 0), (1, 2, 2)]:
            with self.subTest(channels=channels, width=width, frames=frames):
                with tempfile.TemporaryDirectory() as directory:
                    path = Path(directory) / "input.wav"
                    with wave.open(str(path), "wb") as output:
                        output.setparams((channels, width, 48000, 0, "NONE", "not compressed"))
                        output.writeframes(bytes(channels * width * frames))
                    with self.assertRaisesRegex(ValueError, "mono 16-bit PCM with at least 3 frames"):
                        measure(path)


if __name__ == "__main__":
    unittest.main()
