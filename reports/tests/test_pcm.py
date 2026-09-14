"""Regression tests for the C audio applications; builds in a temporary directory."""
import io
import os
import re
import shutil
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "pcm"


def wav_bytes(values, fs=48000, channels=1, width=2):
    values = np.asarray(values)
    data = io.BytesIO()
    with wave.open(data, "wb") as output:
        output.setparams((channels, width, fs, 0, "NONE", "not compressed"))
        output.writeframes(values.astype({1: "u1", 2: "<i2", 4: "<i4"}[width]).tobytes())
    return data.getvalue()


def samples(data):
    with wave.open(io.BytesIO(data), "rb") as source:
        return np.frombuffer(source.readframes(source.getnframes()), dtype="<i2")


class AudioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.build = Path(cls.temp.name)
        for name in ["rate", "ft", "fft", "dft", "seq", "mix", "fir", "thru", "sin", "sin2", "synth", "ip", "iir"]:
            subprocess.run(["cc", "-std=c99", "-D_DEFAULT_SOURCE", "-D_POSIX_C_SOURCE=200809L", "-O2",
                            "-I", str(SRC), str(SRC / (name + ".c")), str(SRC / "pcm.c"),
                            "-lm", "-o", str(cls.build / name)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_app(self, name, *args, data=None):
        return subprocess.run([str(self.build / name), *map(str, args)], input=data,
                              capture_output=True, cwd=self.build, timeout=30)

    def fixture(self, name, data):
        path = self.build / name
        path.write_bytes(data)
        return path

    def test_rate_updates_consistent_header_without_changing_samples(self):
        source = wav_bytes([100, -200, 300, -400])
        result = self.run_app("rate", "2", data=source)
        self.assertEqual(result.returncode, 0, result.stderr)
        fs, byte_rate = struct.unpack_from("<II", result.stdout, 24)
        self.assertEqual((fs, byte_rate), (96000, 192000))
        np.testing.assert_array_equal(samples(result.stdout), samples(source))

    def test_rate_rejects_invalid_or_overflowing_factors(self):
        for factor in ["0", "-1", "nan", "inf", "1e100", "2oops", "0.000000001"]:
            with self.subTest(factor=factor):
                result = self.run_app("rate", factor, data=wav_bytes([100, 200]))
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")

    def test_ft_roundtrip_with_freed_memory_perturbed(self):
        x = np.round(10000 * np.sin(2 * np.pi * 7 * np.arange(256) / 256)).astype("<i2")
        source = self.fixture("fft-input.wav", wav_bytes(x))
        result = subprocess.run([str(self.build / "ft"), str(source), "256", "0"],
                                cwd=self.build, capture_output=True,
                                env={**os.environ, "MALLOC_PERTURB_": "165"}, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        errors = re.findall(r"max [|]IFFT.*?= ([0-9.eE+-]+)", result.stderr.decode())
        self.assertEqual(len(errors), 1)
        self.assertLess(float(errors[0]), 1e-10)
        restored = np.loadtxt(self.build / "ifft.txt")[:, 1]
        np.testing.assert_allclose(restored, x / 32768, atol=1e-7)

    def test_analysis_rejects_invalid_counts_and_offsets(self):
        source = self.fixture("analysis-input.wav", wav_bytes(np.zeros(256)))
        for app in ["dft", "fft", "ft"]:
            for count, start in [("0", "0"), ("-1", "0"), ("abc", "0"), ("32x", "0"),
                                 ("999999999999999999", "0"), ("512", "0"), ("64", "-1"), ("64", "oops")]:
                with self.subTest(app=app, count=count, start=start):
                    result = self.run_app(app, source, count, start)
                    self.assertNotEqual(result.returncode, 0)

    def test_seq_and_mix_reject_mismatched_pcm(self):
        first = self.fixture("first.wav", wav_bytes([100, 200]))
        for app in ["seq", "mix"]:
            for data in [wav_bytes([100, 200], fs=44100), wav_bytes([100, 200], channels=2),
                         wav_bytes([128, 129], width=1)]:
                second = self.fixture("second.wav", data)
                result = self.run_app(app, first, second)
                self.assertNotEqual(result.returncode, 0, app)
                self.assertEqual(result.stdout, b"")

    def test_fir_preserves_impulse_response_gain(self):
        ir = self.fixture("fir-ir.wav", wav_bytes([16384, 8192]))
        result = self.run_app("fir", ir, data=wav_bytes([1000, 1000, 1000]))
        self.assertEqual(result.returncode, 0, result.stderr)
        np.testing.assert_array_equal(samples(result.stdout), [500, 750, 750, 250])

    def test_fir_rejects_incompatible_ir(self):
        for ir_data in [wav_bytes([1, 2], fs=44100), wav_bytes([1, 2], channels=2)]:
            ir = self.fixture("bad-ir.wav", ir_data)
            result = self.run_app("fir", ir, data=wav_bytes([1000, 2000]))
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b"")

    def test_odd_sized_8bit_data_does_not_add_a_sample(self):
        result = self.run_app("sin2", "1", "8", "1", "3")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(struct.unpack_from("<I", result.stdout, 40)[0], 3)
        self.assertEqual(len(result.stdout), 48)
        self.assertEqual(struct.unpack_from("<I", result.stdout, 4)[0], 40)
        with wave.open(io.BytesIO(result.stdout), "rb") as source:
            self.assertEqual(source.getnframes(), 3)

    def test_invalid_pcm_attributes_are_rejected(self):
        for bit in [0, 7, 40]:
            result = self.run_app("sin2", 440, bit, 1, 48000)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout, b"")
        valid = wav_bytes([1, 2, 3, 4])
        for offset, fmt, value in [(20, "H", 3), (22, "H", 3), (24, "I", 0),
                                   (28, "I", 0), (32, "H", 0), (34, "H", 40)]:
            bad = bytearray(valid)
            struct.pack_into("<" + fmt, bad, offset, value)
            result = self.run_app("thru", data=bad)
            self.assertEqual(result.returncode, 1, (offset, result.stderr))
            self.assertEqual(result.stdout, b"")

    def test_thru_skips_padded_metadata_chunks_on_stdin(self):
        source = wav_bytes([100, -200, 300])
        metadata = b"JUNK" + struct.pack("<I", 3) + b"abc\0"
        data = bytearray(source[:36] + metadata + source[36:])
        struct.pack_into("<I", data, 4, len(data) - 8)
        result = self.run_app("thru", data=data)
        self.assertEqual(result.returncode, 0, result.stderr)
        np.testing.assert_array_equal(samples(result.stdout), [100, -200, 300])

    def test_make_builds_all_programs_with_local_library(self):
        sandbox = self.build / "make-source"
        sandbox.mkdir(exist_ok=True)
        for path in [*SRC.glob("*.c"), *SRC.glob("*.h"), SRC / "Makefile"]:
            shutil.copy2(path, sandbox / path.name)
        result = subprocess.run(["make", "-C", str(sandbox)], capture_output=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        for name in ["sin", "sin2", "thru", "rate", "wave", "synth", "seq", "mix", "dft", "ft", "fft", "ip", "iir", "fir"]:
            self.assertTrue((sandbox / "build" / name).is_file(), name)
        result = subprocess.run([str(sandbox / "build" / "sin2")], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_synth_suppresses_aliasing_and_ends_at_zero(self):
        result = self.run_app("synth", "10000")
        self.assertEqual(result.returncode, 0)
        x = samples(result.stdout).astype(float) / 32768
        self.assertEqual(x[0], 0)
        self.assertEqual(x[-1], 0)
        steady = x[14400:28800]
        n = np.arange(len(steady))
        basis = np.column_stack([np.sin(2*np.pi*10000*n/48000), np.cos(2*np.pi*10000*n/48000), np.ones(len(n))])
        residual = steady - basis @ np.linalg.lstsq(basis, steady, rcond=None)[0]
        self.assertLess(np.sqrt(np.mean(residual**2)), 3e-5)

    def test_iir_accepts_signed_feedback_and_rejects_unstable_settings(self):
        impulse = np.zeros(4000, dtype="<i2")
        impulse[0] = 10000
        result = self.run_app("iir", "-0.4", "0", "0", data=wav_bytes(impulse))
        self.assertEqual(result.returncode, 0, result.stderr)
        out = samples(result.stdout)
        self.assertEqual(out[0], 5000)
        self.assertEqual(out[1009], -2000)
        for args in [("0.5", "0.3", "0.2"), ("nan", "0", "0"), ("0.4",)]:
            result = self.run_app("iir", *args, data=wav_bytes(impulse))
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout, b"")

    def test_ip_supports_32bit_and_rejects_invalid_duration(self):
        result = self.run_app("ip", "0.1", "32")
        self.assertEqual(result.returncode, 0)
        with wave.open(io.BytesIO(result.stdout), "rb") as source:
            self.assertEqual(source.getsampwidth(), 4)
            self.assertEqual(source.getnframes(), 4800)
        for value in ["nan", "inf", "-1", "0", "1e100", "1x"]:
            result = self.run_app("ip", value)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout, b"")

    def test_generators_reject_invalid_frequency(self):
        for app in ["sin", "sin2", "synth"]:
            for frequency in ["0", "-1", "24000", "nan", "440x", "A4x"]:
                with self.subTest(app=app, frequency=frequency):
                    result = self.run_app(app, frequency)
                    self.assertEqual(result.returncode, 1)
                    self.assertEqual(result.stdout, b"")


if __name__ == "__main__":
    unittest.main()
