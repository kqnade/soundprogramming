import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReproductionTests(unittest.TestCase):
    def test_all_experiments_generate_consistent_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "assets"
            result = subprocess.run([sys.executable, str(ROOT / "reproduce.py"), "--output", str(output)],
                                    capture_output=True, text=True, timeout=240)
            self.assertEqual(result.returncode, 0, result.stderr)
            data = json.loads((output / "results.json").read_text())
            self.assertEqual(len(data["pcm"]), 7)
            self.assertEqual(data["generation"]["cadence_frames"], 144000)
            self.assertLess(data["fourier"]["fft_numpy_max_error"], 1e-8)
            self.assertLess(data["fourier"]["idft_max_error"], 1e-10)
            self.assertLess(data["filter"]["fir_iir_max_error"], 0.001)
            self.assertAlmostEqual(data["synth"]["speed_2x"]["duration_s"], 0.5)
            self.assertAlmostEqual(data["synth"]["pitch_100cent"]["peak_frequency_hz"], 466.164, delta=1)
            for name in ["pcm/headers.txt", "generation/waveforms.png", "fourier/spectrum.png",
                         "fourier/timing.png", "filter/response.png", "synth/time_pitch.png"]:
                self.assertTrue((output / name).is_file(), name)


if __name__ == "__main__":
    unittest.main()
