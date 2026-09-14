"""Build the C programs and reproduce the report measurements and figures.

Usage: python reports/reproduce.py [--output DIRECTORY]
Dependencies: numpy, matplotlib, imageio-ffmpeg (or an ffmpeg executable on PATH).
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import tempfile
import wave

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from analyze_audio import measure

ROOT = Path(__file__).resolve().parent
BIN = ROOT / "pcm" / "build"


def read_pcm(path):
    with wave.open(str(path), "rb") as source:
        fs, width, channels = source.getframerate(), source.getsampwidth(), source.getnchannels()
        raw = source.readframes(source.getnframes())
    if width == 1:
        x = (np.frombuffer(raw, dtype="u1").astype(float) - 128) / 128
    elif width in (2, 4):
        x = np.frombuffer(raw, dtype=f"<i{width}").astype(float) / 2 ** (8 * width - 1)
    elif width == 3:
        b = np.frombuffer(raw, dtype="u1").reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = ((v ^ 0x800000) - 0x800000) / 8388608
    else:
        raise ValueError("unsupported PCM width")
    return fs, x.reshape(-1, channels)


def write_pcm32(path, x, fs=48000):
    raw = np.clip(np.rint(np.asarray(x) * 2147483648), -2147483648, 2147483647).astype("<i4")
    with wave.open(str(path), "wb") as output:
        output.setparams((1, 4, fs, 0, "NONE", "not compressed"))
        output.writeframes(raw.tobytes())


def spectrum(path):
    fs, data = read_pcm(path)
    x = data[:, 0]
    window = np.hanning(len(x))
    return np.fft.rfftfreq(len(x), 1/fs), 2 * np.abs(np.fft.rfft(x * window)) / window.sum()


def save_figure(figure, path):
    figure.tight_layout()
    figure.savefig(path, dpi=150)
    plt.close(figure)


def reproduce(output):
    output.mkdir(parents=True, exist_ok=True)
    for folder in ["pcm", "generation", "fourier", "filter", "synth"]:
        (output / folder).mkdir(exist_ok=True)
    logs = []
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)

        def run(args, *, source=None, destination=None, cwd=work):
            result = subprocess.run(list(map(str, args)), cwd=cwd,
                                    input=None if source is None else Path(source).read_bytes(),
                                    capture_output=True, timeout=180)
            import shlex
            command = shlex.join(list(map(str, args)))
            if source is not None:
                command += " < " + shlex.quote(str(source))
            if destination is not None:
                command += " > " + shlex.quote(str(destination))
            logs.extend(["$ " + command, result.stderr.decode(errors="replace")])
            if result.returncode:
                raise RuntimeError(f"{command}\n{result.stderr.decode(errors='replace')}")
            if destination is not None:
                Path(destination).write_bytes(result.stdout)
            return result

        build = run(["make", "-B", "-C", ROOT / "pcm"], cwd=ROOT)
        (output / "build.log").write_text(build.stdout.decode() + build.stderr.decode())
        results = {"environment": {"platform": platform.platform(), "python": platform.python_version(),
                                  "numpy": np.__version__, "matplotlib": matplotlib.__version__,
                                  "utc": datetime.now(timezone.utc).isoformat(),
                                  "compiler": subprocess.check_output(["cc", "--version"], text=True).splitlines()[0]}}
        # Wk2: attributes, binary headers, and playback rate.
        p = output / "pcm"
        attributes = [(8, 1, 48000), (16, 1, 48000), (24, 1, 48000), (32, 1, 48000),
                      (16, 2, 48000), (16, 1, 8000), (16, 1, 44100)]
        results["pcm"] = []
        dumps = []
        for bit, ch, fs in attributes:
            path = p / f"sin_{bit}_{ch}_{fs}.wav"
            run([BIN / "sin2", 440, bit, ch, fs], destination=path)
            header = path.read_bytes()[:44]
            _, channels, rate, byte_rate, align, bits = struct.unpack_from("<HHIIHH", header, 20)
            results["pcm"].append({"file": path.name, "bits": bits, "channels": channels,
                                   "fs": rate, "byte_rate": byte_rate, "block_align": align,
                                   "data_bytes": struct.unpack_from("<I", header, 40)[0],
                                   "file_bytes": path.stat().st_size})
            dump = run(["hexdump", "-C", "-n", "44", path])
            dumps.append(path.name + "\n" + dump.stdout.decode())
        (p / "headers.txt").write_text("\n\n".join(dumps) + "\n")
        pure = p / "sin_16_1_48000.wav"
        rates = []
        for rate in [.25, .5, 1, 2, 4]:
            dest = p / f"rate_{rate:g}.wav"
            run([BIN / "rate", rate], source=pure, destination=dest)
            entry = measure(dest)
            entry["file"], entry["factor"] = dest.name, rate
            rates.append(entry)
        results["rates"] = rates
        fig, ax = plt.subplots(figsize=(9, 4))
        for row in rates:
            f, a = spectrum(p / row["file"])
            ax.plot(f, a, label=f'{row["factor"]:g}x')
        ax.set(xlim=(0, 2000), xlabel="Frequency [Hz]", ylabel="Amplitude", title="Playback rate and pitch")
        ax.legend()
        save_figure(fig, p / "rates.png")

        # Wk3: additive synthesis, concatenation, and mixing.
        g = output / "generation"
        for note in ["C", "D", "E", "F", "G", "A", "B", "C5"]:
            run([BIN / "synth", note], destination=g / (note + ".wav"))
        run([BIN / "seq", g / "C.wav", g / "D.wav", g / "E.wav"], destination=g / "C-D-E.wav")
        for name, notes in [("CEG", ["C", "E", "G"]), ("BFG", ["B", "F", "G"])]:
            run([BIN / "mix", *[g / (note + ".wav") for note in notes]], destination=g / (name + ".wav"))
        run([BIN / "seq", g / "CEG.wav", g / "BFG.wav", g / "CEG.wav"], destination=g / "cadence.wav")
        fs, chord = read_pcm(g / "CEG.wav")
        average = sum(read_pcm(g / (n + ".wav"))[1] for n in ["C", "E", "G"]) / 3
        results["generation"] = {"cadence_frames": len(read_pcm(g / "cadence.wav")[1]),
                                  "sequence_frames": len(read_pcm(g / "C-D-E.wav")[1]),
                                  "mix_frames": len(chord), "mix_peak": float(np.max(np.abs(chord))),
                                  "mix_max_error": float(np.max(np.abs(chord - average)))}
        fig, axs = plt.subplots(3, 1, figsize=(10, 8))
        for ax, name in zip(axs, ["A", "C-D-E", "cadence"]):
            fs, x = read_pcm(g / (name + ".wav"))
            ax.plot(np.arange(len(x))/fs, x[:, 0], linewidth=.3)
            ax.set(title=name, xlabel="Time [s]", ylabel="Amplitude")
        save_figure(fig, g / "waveforms.png")
        fig, ax = plt.subplots(figsize=(10, 4))
        for name in ["A", "CEG"]:
            f, a = spectrum(g / (name + ".wav"))
            ax.plot(f, a, label=name)
        ax.set(xlim=(0, 2500), xlabel="Frequency [Hz]", ylabel="Amplitude (Hann corrected)")
        ax.legend()
        save_figure(fig, g / "spectrum.png")

        # Wk5/Wk6: compare the actual C implementations with NumPy and with each other.
        fourier = output / "fourier"
        timings = []
        for n in [64, 256, 1024, 4096]:
            values = []
            for trial in range(5):
                result = run([BIN / "ft", pure, n, 0])
                text = result.stderr.decode()
                values.append({key: float(re.search(rf"^{key}\s+: ([0-9.eE+-]+) s", text, re.M)[1])
                               for key in ["DFT", "IDFT", "FFT", "IFFT"]})
                (fourier / f"ft_{n}_{trial}.log").write_text(text)
                if n == 1024 and trial == 0:
                    for name in ["wf", "dft", "idft", "fft", "ifft"]:
                        shutil.copy2(work / (name + ".txt"), fourier / (name + ".txt"))
            timings.append({"N": n, "runs": values,
                            "median_s": {k: float(np.median([v[k] for v in values])) for k in values[0]},
                            "min_s": {k: min(v[k] for v in values) for k in values[0]},
                            "max_s": {k: max(v[k] for v in values) for k in values[0]}})
        run([BIN / "fft", pure, 1024, 0])
        fft = np.loadtxt(work / "fft.txt")[:, 1]
        run([BIN / "dft", pure, 1024, 0])
        dft = np.loadtxt(work / "dft.txt")[:, 1]
        inverse = np.loadtxt(work / "idft.txt")[:, 1]
        for name in ["dft", "idft"]:
            shutil.copy2(work / (name + ".txt"), fourier / ("standalone_" + name + ".txt"))
        x = read_pcm(pure)[1][:1024, 0]
        results["fourier"] = {"timings": timings,
                               "fft_numpy_max_error": float(np.max(np.abs(fft - abs(np.fft.fft(x)) ))),
                               "dft_fft_magnitude_max_error": float(np.max(np.abs(dft - fft))),
                               "idft_max_error": float(np.max(np.abs(inverse - x))),
                               "ifft_max_error": float(np.max(np.abs(np.loadtxt(fourier / "ifft.txt")[:, 1] - x)))}
        fig, axs = plt.subplots(2, 1, figsize=(10, 7))
        f = np.arange(1024) * 48000 / 1024
        axs[0].plot(f[:513], dft[:513], label="DFT")
        axs[0].plot(f[:513], fft[:513], "--", label="FFT")
        axs[0].set(xlim=(0, 2000), xlabel="Frequency [Hz]", ylabel="Unnormalized magnitude")
        axs[0].legend()
        axs[1].plot(np.arange(1024)/48000, x, label="input")
        axs[1].plot(np.arange(1024)/48000, inverse, "--", label="IDFT")
        axs[1].plot(np.arange(1024)/48000, np.loadtxt(fourier / "ifft.txt")[:, 1], ":", label="IFFT")
        axs[1].set(xlabel="Time [s]", ylabel="Amplitude")
        axs[1].legend()
        save_figure(fig, fourier / "spectrum.png")
        fig, ax = plt.subplots(figsize=(9, 5))
        for key in ["DFT", "IDFT", "FFT", "IFFT"]:
            ax.loglog([v["N"] for v in timings], [v["median_s"][key] for v in timings], "o-", label=key)
        ax.set(xlabel="N", ylabel="Median function time [s]", title="5 runs per N; file I/O excluded")
        ax.legend()
        save_figure(fig, fourier / "timing.png")

        # Wk11: calibrated IR, finite-length FIR, and signed feedback comparisons.
        filt = output / "filter"
        run([BIN / "ip", 2, 32], destination=filt / "impulse.wav")
        amplitude = read_pcm(filt / "impulse.wav")[1][0, 0]
        responses = {}
        for name, coefficients in [("default", []), ("weak", [.2, .15, .1]), ("signed", [-.4, .3, .2])]:
            path = filt / (name + "_ir_raw.wav")
            run([BIN / "iir", *coefficients], source=filt / "impulse.wav", destination=path)
            responses[name] = read_pcm(path)[1][:, 0] / amplitude
        h = responses["default"]
        write_pcm32(filt / "ir.wav", h)
        t = np.arange(12000) / 48000
        tone = .05 * np.sin(2 * np.pi * 440 * t) * np.sin(np.pi * np.arange(12000)/11999)**2
        write_pcm32(filt / "input.wav", tone)
        length = len(tone) + len(h) - 1
        write_pcm32(filt / "padded_input.wav", np.pad(tone, (0, length - len(tone))))
        run([BIN / "iir"], source=filt / "padded_input.wav", destination=filt / "iir.wav")
        run([BIN / "fir", filt / "ir.wav"], source=filt / "input.wav", destination=filt / "fir.wav")
        yi, yf = [read_pcm(filt / (n + ".wav"))[1][:, 0] for n in ["iir", "fir"]]
        freq = np.fft.rfftfreq(131072, 1/48000)
        theoretical = .5 / (1 - sum(a*np.exp(-2j*np.pi*freq*d/48000) for a, d in zip([.4, .3, .2], [1009, 2011, 3001])))
        results["filter"] = {"impulse_amplitude": float(amplitude), "ir_frames": len(h),
                              "fir_frames": len(yf), "iir_frames": len(yi),
                              "fir_iir_max_error": float(np.max(np.abs(yf-yi))),
                              "fir_iir_rms_error": float(np.sqrt(np.mean((yf-yi)**2))),
                              "ir_sum": float(h.sum()), "theoretical_dc_gain": 5.0,
                              "iir_peak": float(np.max(np.abs(yi))), "fir_peak": float(np.max(np.abs(yf))),
                              "dc_gains": {name: float(values.sum()) for name, values in responses.items()}}
        fig, axs = plt.subplots(2, 1, figsize=(10, 8))
        for name, ir in responses.items():
            H = np.fft.rfft(ir, n=131072)
            axs[0].plot(freq, 20*np.log10(np.maximum(abs(H), 1e-12)), label=name)
        axs[0].plot(freq, 20*np.log10(abs(theoretical)), "--", lw=.8, label="default theory")
        axs[0].set(xlim=(0, 1000), xlabel="Frequency [Hz]", ylabel="Gain [dB]")
        axs[0].legend()
        axs[1].plot(np.arange(length)/48000, yi, label="IIR", lw=.5)
        axs[1].plot(np.arange(length)/48000, yf, "--", label="FIR", lw=.5)
        axs[1].set(xlabel="Time [s]", ylabel="Amplitude")
        axs[1].legend()
        save_figure(fig, filt / "response.png")
        fig, axs = plt.subplots(2, 1, figsize=(10, 6))
        axs[0].plot(np.arange(len(h))/48000, h, lw=.5)
        axs[0].set(xlabel="Time [s]", ylabel="h[n]", title="Calibrated 2 s impulse response")
        axs[1].plot(np.arange(length)/48000, yf-yi, lw=.5)
        axs[1].set(xlabel="Time [s]", ylabel="FIR - IIR")
        save_figure(fig, filt / "impulse_error.png")

        # Wk12/Wk13: synthesis effects.
        synth = output / "synth"
        run([BIN / "synth", "A4"], destination=synth / "source.wav")
        ffmpeg = shutil.which("ffmpeg")
        if not ffmpeg:
            from imageio_ffmpeg import get_ffmpeg_exe
            ffmpeg = get_ffmpeg_exe()
        results["environment"]["ffmpeg"] = subprocess.check_output([ffmpeg, "-version"], text=True).splitlines()[0]
        effects = {"speed_2x": "asetrate=96000,aresample=48000", "tempo_0833": "atempo=0.833",
                   "pitch_100cent": "asetrate=50854,aresample=48000,atempo=0.943874313",
                   "tremolo": "tremolo=f=5:d=0.8", "vibrato": "vibrato=f=5:d=0.5",
                   "lowpass": "lowpass=f=1000:p=2", "echo": "aecho=0.6:0.5:100|200:0.3|0.15"}
        for name, effect in effects.items():
            run([ffmpeg, "-hide_banner", "-nostdin", "-y", "-i", synth / "source.wav", "-af", effect,
                 "-ar", "48000", "-ac", "1", "-c:a", "pcm_s16le", synth / (name + ".wav")])
        results["synth"] = {}
        for name in ["source", *effects]:
            entry = measure(synth / (name + ".wav"))
            entry["file"] = name + ".wav"
            results["synth"][name] = entry
        fig, axs = plt.subplots(4, 2, figsize=(12, 10))
        for row, name in enumerate(["source", "speed_2x", "tempo_0833", "pitch_100cent"]):
            fs, x = read_pcm(synth / (name + ".wav"))
            f, a = spectrum(synth / (name + ".wav"))
            axs[row, 0].plot(np.arange(len(x))/fs, x[:, 0], lw=.3)
            axs[row, 0].set(xlim=(0, 1.25), ylim=(-.4, .4), title=name, xlabel="Time [s]", ylabel="Amplitude")
            axs[row, 1].plot(f, a)
            axs[row, 1].set(xlim=(350, 1000), title=name, xlabel="Frequency [Hz]", ylabel="Amplitude")
        save_figure(fig, synth / "time_pitch.png")
        fig, axs = plt.subplots(3, 1, figsize=(10, 9))
        for name in ["source", "tremolo", "vibrato"]:
            fs, x = read_pcm(synth / (name + ".wav"))
            rms = np.sqrt(np.mean(x[:len(x)//480*480, 0].reshape(-1, 480)**2, axis=1))
            axs[0].plot((np.arange(len(rms))+.5)*.01, rms, label=name)
            f, a = spectrum(synth / (name + ".wav"))
            axs[1].plot(f, 20*np.log10(np.maximum(a, 1e-8)), label=name)
        axs[0].set(xlabel="Time [s]", ylabel="10 ms RMS")
        axs[1].set(xlim=(400, 480), ylim=(-80, -10), xlabel="Frequency [Hz]", ylabel="Amplitude [dBFS]")
        for name in ["source", "lowpass"]:
            f, a = spectrum(synth / (name + ".wav"))
            axs[2].plot(f, 20*np.log10(np.maximum(a, 1e-8)), label=name)
        axs[2].set(xlim=(0, 5500), ylim=(-90, 0), xlabel="Frequency [Hz]", ylabel="Amplitude [dBFS]")
        for ax in axs:
            ax.legend()
        save_figure(fig, synth / "modulation_filter.png")
        fig, axs = plt.subplots(2, 1, figsize=(10, 6))
        for name in ["source", "echo"]:
            fs, x = read_pcm(synth / (name + ".wav"))
            axs[0].plot(np.arange(len(x))/fs, x[:, 0], lw=.3, label=name)
            rms = np.sqrt(np.mean(x[:len(x)//480*480, 0].reshape(-1, 480)**2, axis=1))
            axs[1].plot((np.arange(len(rms))+.5)*.01, rms, label=name)
        axs[0].set(xlabel="Time [s]", ylabel="Amplitude")
        axs[1].set(xlim=(.7, 1.2), xlabel="Time [s]", ylabel="10 ms RMS")
        for ax in axs:
            ax.legend()
        save_figure(fig, synth / "echo.png")
        f, a = spectrum(synth / "source.wav")
        _, b = spectrum(synth / "lowpass.wav")
        results["synth_details"] = {"lowpass_attenuation_db": {str(k): float(20*np.log10(b[k]/a[k])) for k in [440, 1320, 2200]},
                                    "echo_tail_rms": float(np.sqrt(np.mean(read_pcm(synth / "echo.wav")[1][48000:]**2)))}
        results["source_sha256"] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in [*sorted((ROOT / "pcm").glob("*.c")), *sorted((ROOT / "pcm").glob("*.h")),
                                              ROOT / "pcm" / "Makefile", ROOT / "reproduce.py", ROOT / "analyze_audio.py"]}
        (output / "results.json").write_text(json.dumps(results, indent=2, ensure_ascii=False) + "\n")
        text = "\n".join(logs).replace(str(output), "$ASSETS").replace(str(BIN), "$BIN").replace(str(work), "$WORK")
        (output / "commands.log").write_text(text)
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "assets")
    args = parser.parse_args()
    reproduce(args.output.resolve())
    print("All experiment outputs generated.")
