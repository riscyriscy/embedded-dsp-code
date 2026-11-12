"""One lowpass spec, designed as FIR and as IIR, so the costs can be compared like for like.
Spec: fs = 48 kHz, passband edge 1000 Hz (ripple <= 1 dB), stopband edge 1500 Hz (>= 60 dB down).
Then the same spec scaled down to 100 / 150 Hz, and the effect of rounding the coefficients."""
import warnings
import numpy as np
import scipy
from scipy import signal

warnings.simplefilter("ignore")
FS, RP, RS = 48000.0, 1.0, 60.0
w = np.linspace(0, FS / 2, 200001)


def ripple_atten(H, fp, fst):
    mag = 20 * np.log10(np.maximum(np.abs(H), 1e-12))
    pb, sb = mag[w <= fp], mag[w >= fst]
    return pb.max() - pb.min(), -sb.max()


def gd_sos(sos, fp):
    f = np.linspace(1.0, fp, 4001)
    _, H = signal.sosfreqz(sos, worN=f, fs=FS)
    gd = -np.gradient(np.unwrap(np.angle(H)), 2 * np.pi * f / FS)
    return gd.min(), gd.max()


def design_fir(fp, fst, lo=11, hi=400, step=2):
    dev_p = (10 ** (RP / 20) - 1) / (10 ** (RP / 20) + 1)
    dev_s = 10 ** (-RS / 20)
    for n in range(lo, hi, step):
        h = signal.remez(n, [0, fp, fst, FS / 2], [1, 0], weight=[1 / dev_p, 1 / dev_s], fs=FS, maxiter=100)
        _, H = signal.freqz(h, worN=w, fs=FS)
        r, a = ripple_atten(H, fp, fst)
        if r <= RP and a >= RS:
            return n, h, a


def design_iir(kind, fp, fst):
    if kind == "Butterworth":
        N, wn = signal.buttord(fp, fst, RP, RS, fs=FS)
        return N, signal.butter(N, wn, output="sos", fs=FS)
    if kind == "Chebyshev I":
        N, wn = signal.cheb1ord(fp, fst, RP, RS, fs=FS)
        return N, signal.cheby1(N, RP, wn, output="sos", fs=FS)
    N, wn = signal.ellipord(fp, fst, RP, RS, fs=FS)
    return N, signal.ellip(N, RP, RS, wn, output="sos", fs=FS)


def settle(sos):
    imp = signal.sosfilt(sos, np.r_[1.0, np.zeros(19999)])
    return int(np.where(np.abs(imp) > 1e-3 * np.abs(imp).max())[0][-1]) + 1


def radius(sos):
    return np.abs(np.concatenate([np.roots(s[3:]) for s in sos])).max()


FP, FST = 1000.0, 1500.0
n, h, fir_atten = design_fir(FP, FST)
print("1 kHz / 1.5 kHz at fs = 48 kHz, <= 1 dB ripple, >= 60 dB stopband")
print("design        size        mults  delay, samples   settles, samples")
print(f"{'FIR (PM)':<13s} {f'{n} taps':<11s} {(n + 1) // 2:5d}  {f'{(n - 1) // 2} (constant)':<15s}{n:6d}")
for kind in ("Butterworth", "Chebyshev I", "Elliptic"):
    N, sos = design_iir(kind, FP, FST)
    g0, g1 = gd_sos(sos, FP)
    print(f"{kind:<13s} {f'{len(sos)} biquads':<11s} {5 * len(sos):5d}  {f'{g0:.1f} to {g1:.1f}':<15s}{settle(sos):6d}")
print("(FIR mults use symmetry; IIR is 5 mults per biquad; delay is across")
print(" the passband; settles = samples above 0.1% of the peak response)")

print()
_, Hq = signal.freqz(np.round(h * 32768.0) / 32768.0, worN=w, fs=FS)
print(f"FIR taps rounded to Q15: stopband {ripple_atten(Hq, FP, FST)[1]:.1f} dB (was {fir_atten:.1f})")
N, sos = design_iir("Elliptic", FP, FST)
q = np.round(sos * 16384.0) / 16384.0
_, Hs = signal.sosfreqz(q, worN=w, fs=FS)
r, a = ripple_atten(Hs, FP, FST)
print(f"Elliptic, 14 fractional bits: stopband {a:.1f} dB, pole radius {radius(q):.6f}")

print()
FP2, FST2 = 100.0, 150.0
N2, sos2 = design_iir("Elliptic", FP2, FST2)
n2, h2, a2 = design_fir(FP2, FST2, lo=1901, hi=2401, step=10)
print("Same spec scaled down to 100 / 150 Hz:")
print(f"  elliptic: {len(sos2)} biquads, {5 * len(sos2)} mults; FIR: {n2} taps (10-tap steps)")
print(f"  as designed: pole radius {radius(sos2):.6f}")
for frac in (14, 12):
    q = np.round(sos2 * 2.0 ** frac) / 2.0 ** frac
    _, H2 = signal.sosfreqz(q, worN=w, fs=FS)
    r, a = ripple_atten(H2, FP2, FST2)
    rr = "nan" if not np.isfinite(r) else f"{r:.1f}"
    print(f"  {frac} fractional bits: pole radius {radius(q):.6f}")
    print(f"    stopband {a:.1f} dB, passband ripple {rr} dB")
print(f"scipy {scipy.__version__}, numpy {np.__version__}")
