#!/usr/bin/env python3
# plot_ft.py — ft.c の出力（wf.txt / dft.txt / fft.txt / idft.txt / ifft.txt）をプロット
# Usage: python3 plot_ft.py [DATA_DIR]
import sys, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

d = sys.argv[1] if len(sys.argv) > 1 else "."

def load(fn):
    path = os.path.join(d, fn)
    if not os.path.exists(path): return None
    return np.loadtxt(path)

plt.rcParams.update({
    "font.size": 11,
    "axes.grid": True,
    "grid.alpha": 0.3,
})

# --- 波形 2x1: 入力波形 + IFFT 復元波形 ---
wf = load("wf.txt")
ifft = load("ifft.txt")
idft = load("idft.txt")
if wf is not None:
    fig, axes = plt.subplots(2, 1, figsize=(8, 6), sharex=True)
    axes[0].plot(wf[:,0]*1000, wf[:,1], lw=0.8, color="C0")
    axes[0].set_title("Input waveform  x[n]  (A.wav, N=1024)")
    axes[0].set_ylabel("amplitude")
    if idft is not None:
        axes[1].plot(idft[:,0]*1000, idft[:,1], lw=0.8, color="C1",
                     label="IDFT (O(N^2))")
    if ifft is not None:
        axes[1].plot(ifft[:,0]*1000, ifft[:,1], lw=0.8, color="C2",
                     linestyle="--", label="IFFT (O(N log N))")
    axes[1].set_title("Reconstructed waveform (IDFT vs IFFT)")
    axes[1].set_xlabel("time [ms]")
    axes[1].set_ylabel("amplitude")
    axes[1].legend(loc="upper right")
    fig.tight_layout()
    fig.savefig(os.path.join(d, "ft_waveform.png"), dpi=120)
    plt.close(fig)
    print("saved:", os.path.join(d, "ft_waveform.png"))

# --- スペクトル: DFT と FFT を比較して stem ---
dft = load("dft.txt")
fft = load("fft.txt")
if dft is not None and fft is not None:
    fig, axes = plt.subplots(2, 1, figsize=(8, 6), sharex=True)
    half = len(dft) // 2   # ナイキスト以下を表示
    axes[0].stem(dft[:half,0], dft[:half,1], basefmt=" ",
                 linefmt="C0-", markerfmt="C0o")
    axes[0].set_title("DFT spectrum  |X_d[k]|  (O(N^2))")
    axes[1].stem(fft[:half,0], fft[:half,1], basefmt=" ",
                 linefmt="C2-", markerfmt="C2o")
    axes[1].set_title("FFT spectrum  |X_f[k]|  (O(N log N))")
    axes[1].set_xlabel("frequency [Hz]")
    for ax in axes: ax.set_ylabel("|X_k|")
    fig.tight_layout()
    fig.savefig(os.path.join(d, "ft_spectrum.png"), dpi=120)
    plt.close(fig)
    print("saved:", os.path.join(d, "ft_spectrum.png"))

# --- 性能比較: N を変えたときの処理時間 ---
# (実測値; ft.c を N=64,256,1024,4096,8192 で実行して取得)
Ns      = [64, 256, 1024, 4096, 8192]
t_dft   = [2.03e-4, 3.15e-3, 7.51e-2, 1.14e+0, 4.79e+0]
t_idft  = [4.01e-4, 3.40e-3, 6.27e-2, 1.35e+0, 4.82e+0]
t_fft   = [5.96e-6, 2.19e-5, 8.89e-5, 4.52e-4, 9.38e-4]
t_ifft  = [6.91e-6, 2.31e-5, 9.70e-5, 4.62e-4, 1.22e-3]

fig, ax = plt.subplots(figsize=(8, 5))
ax.loglog(Ns, t_dft,  "o-", label="DFT  (O(N^2))",       color="C0")
ax.loglog(Ns, t_idft, "s-", label="IDFT (O(N^2))",       color="C1")
ax.loglog(Ns, t_fft,  "o-", label="FFT  (O(N log N))",  color="C2")
ax.loglog(Ns, t_ifft, "s-", label="IFFT (O(N log N))",  color="C3")

# 理論カーブのガイド
Ns_fine = np.array([64, 8192])
ax.loglog(Ns_fine, (Ns_fine/64.0)**2 * 2.0e-4, ":", color="C0", alpha=0.5)
ax.loglog(Ns_fine, (Ns_fine/64.0) * np.log2(Ns_fine/64.0) * 6.0e-6, ":", color="C2", alpha=0.5)

ax.set_xlabel("N (number of samples)")
ax.set_ylabel("processing time [s]")
ax.set_title("DFT/IDFT vs FFT/IFFT — processing time scaling")
ax.legend(loc="upper left")
fig.tight_layout()
fig.savefig(os.path.join(d, "ft_timing.png"), dpi=120)
plt.close(fig)
print("saved:", os.path.join(d, "ft_timing.png"))