"""I/Q constellations and the I(t), Q(t) waveforms they produce: BPSK, QPSK, 16-QAM (all RRC-shaped)
and BLE-style GFSK. Writes four figures and prints the measured numbers quoted in the book.
Usage: python3 iq_waveforms.py [output_dir]"""
import sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = sys.argv[1] if len(sys.argv) > 1 else "."
SPS, ALPHA, SPAN = 8, 0.35, 8          # samples per symbol, RRC roll-off, filter span in symbols
BLUE, RED, GREY = "#3a6ea5", "#d9534f", "#909090"


def rrc(alpha, sps, span):
    t = (np.arange(-span * sps // 2, span * sps // 2 + 1)) / sps
    h = np.zeros_like(t)
    for i, x in enumerate(t):
        if abs(x) < 1e-9:
            h[i] = 1 - alpha + 4 * alpha / np.pi
        elif abs(abs(x) - 1 / (4 * alpha)) < 1e-9:
            h[i] = alpha / np.sqrt(2) * ((1 + 2 / np.pi) * np.sin(np.pi / (4 * alpha)) + (1 - 2 / np.pi) * np.cos(np.pi / (4 * alpha)))
        else:
            h[i] = (np.sin(np.pi * x * (1 - alpha)) + 4 * alpha * x * np.cos(np.pi * x * (1 + alpha))) / (np.pi * x * (1 - (4 * alpha * x) ** 2))
    return h / np.sqrt(np.sum(h ** 2))        # unit energy, so one symbol per sps samples keeps average power at 1


H = rrc(ALPHA, SPS, SPAN)
rng = np.random.default_rng(7)


def shape(symbols):
    up = np.zeros(len(symbols) * SPS, dtype=complex)
    up[::SPS] = symbols
    x = np.convolve(up, H)
    d = (len(H) - 1) // 2
    # delay-compensated; times sqrt(sps) so the average power per sample equals the average symbol power (1)
    return x[d:d + len(up)] * np.sqrt(SPS)


def stats(x, skip=SPAN * SPS):
    y = x[skip:-skip]
    p = np.abs(y) ** 2
    return 10 * np.log10(p.max() / p.mean()), np.abs(y).min() / np.sqrt(p.mean())


qpsk_pts = np.array([(1 + 1j), (-1 + 1j), (-1 - 1j), (1 - 1j)]) / np.sqrt(2)
lv = np.array([-3, -1, 1, 3]) / np.sqrt(10)


def sym_bpsk(n):
    return rng.choice([-1.0, 1.0], n).astype(complex)


def sym_qpsk(n):
    return qpsk_pts[rng.integers(0, 4, n)]


def sym_qam16(n):
    return lv[rng.integers(0, 4, n)] + 1j * lv[rng.integers(0, 4, n)]


def gfsk(bits, h=0.5, bt=0.5):
    """BLE 1M style: Gaussian-filtered NRZ drives the instantaneous frequency, deviation h/(2T)."""
    nrz = np.repeat(2.0 * bits - 1.0, SPS)
    span = 4
    t = np.arange(-span * SPS // 2, span * SPS // 2 + 1) / SPS
    g = np.exp(-0.5 * (2 * np.pi * bt * t / np.sqrt(np.log(2))) ** 2)
    g /= g.sum()
    f = np.convolve(nrz, g)[(len(g) - 1) // 2:][:len(nrz)]
    phase = np.cumsum(np.pi * h * f / SPS)
    return np.exp(1j * phase)


def figure(name, title, pts, symbols_shown, x_all, x_show, ideal, labels, ring=False, path_n=600):
    fig = plt.figure(figsize=(7.2, 3.4), dpi=200)
    gs = fig.add_gridspec(2, 2, width_ratios=[1, 1.55], wspace=0.28, hspace=0.18)
    ax = fig.add_subplot(gs[:, 0])
    seg = x_all[16 * SPS:16 * SPS + path_n * SPS // (1 if ring else 4)]
    ax.plot(seg.real, seg.imag, color=BLUE, lw=0.6, alpha=0.35)
    if pts is not None:
        ax.scatter(pts.real, pts.imag, s=34, color=BLUE, zorder=3)
    ax.axhline(0, color="#cccccc", lw=0.8)
    ax.axvline(0, color="#cccccc", lw=0.8)
    lim = 1.5 if not ring else 1.35
    ax.set_xlim(-lim, lim)
    ax.set_ylim(-lim, lim)
    ax.set_aspect("equal")
    ax.set_xlabel("I")
    ax.set_ylabel("Q")
    ax.set_title(title, fontsize=9)
    ax.tick_params(labelsize=7)
    n = len(x_show)
    tt = np.arange(n) / SPS
    for k, (comp, nm) in enumerate(((x_show.real, "I(t)"), (x_show.imag, "Q(t)"))):
        a = fig.add_subplot(gs[k, 1])
        a.plot(tt, comp, color=BLUE, lw=1.1)
        a.axhline(0, color="#cccccc", lw=0.8)
        for s in range(symbols_shown + 1):
            a.axvline(s, color="#e0e0e0", lw=0.6, ls=":")
        if ideal is not None:
            v = (ideal.real if k == 0 else ideal.imag)
            a.scatter(np.arange(symbols_shown) + 0.0, v, s=14, color=RED, zorder=3)
        a.set_xlim(0, symbols_shown)
        a.set_ylabel(nm, fontsize=8)
        a.tick_params(labelsize=7)
        a.set_ylim(-1.7, 1.7)
        if k == 0:
            a.set_xticklabels([])
            for s, lab in enumerate(labels):
                a.text(s + 0.5, 1.72, lab, ha="center", va="bottom", fontsize=6.5, clip_on=False)
        else:
            a.set_xlabel("Time (symbol periods)", fontsize=8)
    fig.savefig(f"{OUT}/{name}", bbox_inches="tight", facecolor="white")
    plt.close(fig)


N_ALL, N_SHOW = 400, 12
results = []

# BPSK
s_all = sym_bpsk(N_ALL)
x_all = shape(s_all)
st = 20
show = x_all[st * SPS:(st + N_SHOW) * SPS]
figure("fig_iq_bpsk.png", "BPSK", np.array([-1 + 0j, 1 + 0j]), N_SHOW, x_all, show, s_all[st:st + N_SHOW],
       [f"{'1' if v.real > 0 else '0'}" for v in s_all[st:st + N_SHOW]])
big = shape(sym_bpsk(20000))
results.append(("BPSK", 1) + stats(big))

# QPSK
s_all = sym_qpsk(N_ALL)
x_all = shape(s_all)
show = x_all[st * SPS:(st + N_SHOW) * SPS]
def qlab(v):
    # the book's Gray map (Figure: QPSK Constellation): first bit is the I sign, second the Q sign
    return f"{'1' if v.real < 0 else '0'}{'1' if v.imag < 0 else '0'}"
figure("fig_iq_qpsk.png", "QPSK", qpsk_pts, N_SHOW, x_all, show, s_all[st:st + N_SHOW], [qlab(v) for v in s_all[st:st + N_SHOW]])
big = shape(sym_qpsk(20000))
results.append(("QPSK", 2) + stats(big))

# 16-QAM
s_all = sym_qam16(N_ALL)
x_all = shape(s_all)
show = x_all[st * SPS:(st + N_SHOW) * SPS]
pts = np.array([a + 1j * b for a in lv for b in lv])
figure("fig_iq_16qam.png", "16-QAM", pts, N_SHOW, x_all, show, s_all[st:st + N_SHOW], ["" for _ in range(N_SHOW)])
big = shape(sym_qam16(20000))
results.append(("16-QAM", 4) + stats(big))

# GFSK
bits = rng.integers(0, 2, N_ALL)
x_all = gfsk(bits)
show = x_all[st * SPS:(st + N_SHOW) * SPS]
figure("fig_iq_gfsk.png", "GFSK (h = 0.5, BT = 0.5)", None, N_SHOW, x_all, show, None,
       [str(b) for b in bits[st:st + N_SHOW]], ring=True)
g = gfsk(rng.integers(0, 2, 20000))
results.append(("GFSK", 1) + stats(g, skip=4 * SPS))

print(f"RRC alpha {ALPHA}, {SPS} samples/symbol, 20000 random symbols, seed 7")
print("scheme  bits/sym  PAPR, dB  min |x| / rms")
for nm, b, papr, mn in results:
    print(f"{nm:<7s} {b:5d}    {papr:6.2f}    {mn:7.3f}")
