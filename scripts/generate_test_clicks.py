#!/usr/bin/env python3
"""
generate_test_clicks.py
-----------------------
Generates click-track WAV files for testing the DJ prototype.
Each file contains:
  - Strong click at measure starts
  - Medium click at beat-group starts
  - Light click at sub-beat positions

Usage:
    pip install numpy
    python scripts/generate_test_clicks.py --out tests/audio

Output files:
    7_8_2+2+3_120bpm.wav
    7_8_2+3+2_120bpm.wav
    7_8_3+2+2_120bpm.wav
    9_8_2+2+2+3_120bpm.wav
    9_8_2+2+3+2_120bpm.wav
    9_8_2+3+2+2_120bpm.wav
    9_8_3+2+2+2_120bpm.wav
    silent_5s_prefix.wav
    single_click_center.wav
    clicks_at_end.wav
    stereo_left_only.wav
"""

import argparse
import math
import os
import struct
import wave
import sys

SAMPLE_RATE = 44100
DURATION_BARS = 32       # bars per test file
CLICK_FREQ_STRONG = 1000  # Hz – measure start
CLICK_FREQ_MEDIUM = 800   # Hz – group start
CLICK_FREQ_LIGHT  = 600   # Hz – sub-beat
CLICK_DURATION_S  = 0.02  # 20 ms click


def make_click(freq: float, amplitude: float, sample_rate: int,
               duration: float) -> list[float]:
    n = int(sample_rate * duration)
    return [amplitude * math.sin(2 * math.pi * freq * i / sample_rate)
            * max(0.0, 1.0 - i / n)   # linear decay
            for i in range(n)]


def mix_into(buf: list[float], samples: list[float], offset: int) -> None:
    for i, v in enumerate(samples):
        idx = offset + i
        if 0 <= idx < len(buf):
            buf[idx] = max(-1.0, min(1.0, buf[idx] + v))


def write_wav(path: str, samples: list[float], sample_rate: int,
              channels: int = 1) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with wave.open(path, "w") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(2)  # 16-bit
        wf.setframerate(sample_rate)
        packed = struct.pack(f"<{len(samples)}h",
                             *[int(v * 32767) for v in samples])
        wf.writeframes(packed)
    print(f"  Wrote {path}  ({len(samples)/sample_rate:.2f}s)")


def generate_click_track(
    numerator: int,
    denominator: int,
    grouping: list[int],
    bpm: float,
    num_bars: int,
    sample_rate: int = SAMPLE_RATE,
) -> list[float]:
    """Generate a mono click-track for the given time signature."""
    sub_beat_dur = 60.0 / bpm  # seconds per denominator-note
    measure_dur  = sub_beat_dur * numerator
    total_dur    = measure_dur * num_bars
    buf = [0.0] * int(total_dur * sample_rate + sample_rate)

    # Build group-start lookup within a measure
    group_starts = set()
    pos = 0
    for g in grouping:
        group_starts.add(pos)
        pos += g

    for bar in range(num_bars):
        for sb in range(numerator):
            t   = (bar * measure_dur + sb * sub_beat_dur)
            off = int(t * sample_rate)

            if sb == 0:
                click = make_click(CLICK_FREQ_STRONG, 0.9, sample_rate, CLICK_DURATION_S)
            elif sb in group_starts:
                click = make_click(CLICK_FREQ_MEDIUM, 0.6, sample_rate, CLICK_DURATION_S)
            else:
                click = make_click(CLICK_FREQ_LIGHT,  0.3, sample_rate, CLICK_DURATION_S)

            mix_into(buf, click, off)

    return buf[:int(total_dur * sample_rate)]


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate DJ prototype test click tracks")
    parser.add_argument("--out",  default="tests/audio", help="Output directory")
    parser.add_argument("--bpm",  type=float, default=120.0, help="BPM (counts denominator-note)")
    parser.add_argument("--bars", type=int,   default=DURATION_BARS)
    args = parser.parse_args()

    out  = args.out
    bpm  = args.bpm
    bars = args.bars

    specs = [
        (7, 8, [2, 2, 3], f"7_8_2+2+3_{int(bpm)}bpm.wav"),
        (7, 8, [2, 3, 2], f"7_8_2+3+2_{int(bpm)}bpm.wav"),
        (7, 8, [3, 2, 2], f"7_8_3+2+2_{int(bpm)}bpm.wav"),
        (9, 8, [2, 2, 2, 3], f"9_8_2+2+2+3_{int(bpm)}bpm.wav"),
        (9, 8, [2, 2, 3, 2], f"9_8_2+2+3+2_{int(bpm)}bpm.wav"),
        (9, 8, [2, 3, 2, 2], f"9_8_2+3+2+2_{int(bpm)}bpm.wav"),
        (9, 8, [3, 2, 2, 2], f"9_8_3+2+2+2_{int(bpm)}bpm.wav"),
        (4, 4, [1, 1, 1, 1], f"4_4_standard_{int(bpm)}bpm.wav"),
    ]

    print("Generating click tracks …")
    for num, den, grp, fname in specs:
        buf = generate_click_track(num, den, grp, bpm, bars)
        write_wav(os.path.join(out, fname), buf, SAMPLE_RATE)

    # ---- Special test files ------------------------------------------------

    # 5 seconds silence then 7/8 clicks
    print("Special files …")
    silence = [0.0] * (5 * SAMPLE_RATE)
    normal  = generate_click_track(7, 8, [2, 2, 3], bpm, 8)
    write_wav(os.path.join(out, "silent_5s_prefix.wav"),
              silence + normal, SAMPLE_RATE)

    # Single click in center
    total  = 4 * SAMPLE_RATE
    buf    = [0.0] * total
    click  = make_click(CLICK_FREQ_STRONG, 0.9, SAMPLE_RATE, CLICK_DURATION_S)
    mix_into(buf, click, total // 2)
    write_wav(os.path.join(out, "single_click_center.wav"), buf, SAMPLE_RATE)

    # Several strong clicks at the end
    buf   = [0.0] * (4 * SAMPLE_RATE)
    for k in range(5):
        mix_into(buf, make_click(CLICK_FREQ_STRONG, 0.9, SAMPLE_RATE, CLICK_DURATION_S),
                 len(buf) - (5 - k) * int(0.5 * SAMPLE_RATE))
    write_wav(os.path.join(out, "clicks_at_end.wav"), buf, SAMPLE_RATE)

    # Stereo: left channel has clicks, right channel is silent
    mono   = generate_click_track(4, 4, [1, 1, 1, 1], bpm, 8)
    stereo = []
    for v in mono:
        stereo.append(v)    # left
        stereo.append(0.0)  # right silent
    write_wav(os.path.join(out, "stereo_left_only.wav"), stereo, SAMPLE_RATE, channels=2)

    print(f"\nDone. Files written to: {os.path.abspath(out)}/")


if __name__ == "__main__":
    main()
