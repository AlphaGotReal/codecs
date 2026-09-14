#!/usr/bin/env python3
"""Plot GOP size vs mean random-access decode time (CPU vs NVDEC).

Data from out.txt (N=1000), converted to ms. Regenerate with:
    uv run --with matplotlib python plot_gop.py
"""
import matplotlib.pyplot as plt

# (GOP, cpu_mean, cpu_std, nvdec_mean, nvdec_std) in ms
DATA = [
    (3, 48.4, 6.0, 3.5, 1.0),
    (10, 77.7, 23.9, 5.9, 2.0),
    (20, 124.1, 52.3, 9.5, 3.9),
    (30, 172.2, 79.9, 13.0, 6.0),
    (50, 265.8, 135.0, 19.8, 10.3),
    (60, 317.2, 164.9, 24.5, 12.2),
    (100, 491.1, 260.9, 36.7, 20.8),
    (250, 1176.4, 644.8, 90.3, 52.3),
]

gops = [r[0] for r in DATA]
cpu_m = [r[1] for r in DATA]
cpu_s = [r[2] for r in DATA]
nv_m = [r[3] for r in DATA]
nv_s = [r[4] for r in DATA]

fig, ax = plt.subplots()
ax.errorbar(gops, cpu_m, yerr=cpu_s, marker="o", capsize=3, label="CPU (h264)")
ax.errorbar(gops, nv_m, yerr=nv_s, marker="s", capsize=3, label="NVDEC (h264, hwaccel)")
ax.set_xlabel("GOP size")
ax.set_ylabel("Mean random-access decode time (ms, log scale)")
ax.set_title("GOP size vs mean random-access decode time (N=1000)")
ax.set_yscale("log")
ax.legend()
ax.grid(True, alpha=0.3)
fig.tight_layout()
fig.savefig("gop_vs_decode.png", dpi=150)
print("wrote gop_vs_decode.png")
