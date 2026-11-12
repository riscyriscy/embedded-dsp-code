"""Measure the spurious-free dynamic range (SFDR) of the C NCO in nco_demo.c.

Run after building nco_demo_6, nco_demo_8, nco_demo_10 and nco_demo_12 (see nco_demo.c).
Each tone is coherent: phase_inc = k * 2^12 with odd k, so a 2^20-sample record holds exactly k
cycles and the FFT needs no window. SFDR is the carrier minus the largest other bin, in dB.
"""
import subprocess
import numpy as np

N = 1 << 20
FS = 48_000.0
rng = np.random.default_rng(1)
ks = rng.integers(1, N // 2, size=40) | 1                    # 40 random odd bins below Nyquist


def sfdr_db(exe: str, k: int, interp: int) -> float:
    raw = subprocess.run([exe, str(k * 4096), str(interp), str(N)], capture_output=True, check=True).stdout
    x = np.frombuffer(raw, dtype="<i2").astype(np.float64)
    mag = np.abs(np.fft.rfft(x))
    carrier = mag[k]
    mag[k] = 0.0
    mag[0] = 0.0                                              # ignore DC (a table's mean offset)
    return 20.0 * np.log10(carrier / mag.max())


print("table  interp   worst SFDR   median SFDR   (40 random coherent tones, 2^20 samples)")
for bits in (6, 8, 10, 12):
    for interp in (0, 1):
        if interp and bits > 8:
            continue
        v = [sfdr_db(f"./nco_demo_{bits}", int(k), interp) for k in ks]
        print(f"{1 << bits:5d}  {'linear' if interp else 'none  '}   {min(v):7.1f} dB   {np.median(v):8.1f} dB")

# Frequency resolution, rounding error and drift for a 1 kHz tone at 48 kHz.
inc = round(1000.0 / FS * 2**32)
f_actual = inc * FS / 2**32
print()
print(f"frequency step (fs / 2^32):      {FS / 2**32 * 1e6:.3f} uHz")
print(f"phase_inc for 1000 Hz at 48 kHz: {inc}  (exact value {1000.0 / FS * 2**32:.4f})")
print(f"actual frequency:                {f_actual:.9f} Hz  (error {f_actual - 1000.0:+.3e} Hz)")
print(f"phase drift against an ideal 1 kHz after 1 hour: {(f_actual - 1000.0) * 3600 * 360:+.2f} degrees")
inc16 = round(1000.0 / FS * 2**16)
f16 = inc16 * FS / 2**16
print(f"16-bit accumulator instead:      step {FS / 2**16:.4f} Hz, 1000 Hz becomes {f16:.4f} Hz")
