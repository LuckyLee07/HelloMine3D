#!/usr/bin/env python3
"""Generate the project-owned B9 environment, wildlife and footstep WAVs."""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
from pathlib import Path
from typing import Callable


SAMPLE_RATE = 44_100
TAU = math.tau
Renderer = Callable[[float, float, float, float], float]


def envelope(time: float, duration: float, attack: float, release: float) -> float:
    return max(0.0, min(1.0, time / max(attack, 1e-6),
                        (duration - time) / max(release, 1e-6)))


def pulse(time: float, centre: float, width: float) -> float:
    distance = abs(time - centre) / max(width, 1e-6)
    return max(0.0, 1.0 - distance) ** 2


def forest(time: float, duration: float, white: float, smooth: float) -> float:
    env = envelope(time, duration, 0.28, 0.32)
    breeze = 0.68 + 0.18 * math.sin(TAU * 0.43 * time) + 0.12 * math.sin(TAU * 1.17 * time)
    leaves = 0.34 * white * (0.35 + 0.65 * abs(math.sin(TAU * 2.3 * time)))
    return 0.24 * env * (breeze * smooth + leaves)


def river(time: float, duration: float, white: float, smooth: float) -> float:
    env = envelope(time, duration, 0.26, 0.34)
    current = 0.72 * smooth + 0.18 * white
    burble = 0.10 * math.sin(TAU * (96.0 + 8.0 * math.sin(TAU * 0.7 * time)) * time)
    return 0.30 * env * (current + burble)


def coast(time: float, duration: float, white: float, smooth: float) -> float:
    env = envelope(time, duration, 0.32, 0.38)
    swell = 0.58 + 0.30 * math.sin(TAU * 0.36 * time - 0.7)
    wash = 0.70 * smooth + 0.22 * white
    body = 0.08 * math.sin(TAU * 63.0 * time)
    return 0.34 * env * (swell * wash + body)


def sheep(time: float, duration: float, white: float, smooth: float) -> float:
    env = envelope(time, duration, 0.025, 0.13)
    vibrato = 1.0 + 0.035 * math.sin(TAU * 7.0 * time)
    phase = TAU * (205.0 - 48.0 * time / duration) * vibrato * time
    voice = math.sin(phase) + 0.42 * math.sin(phase * 2.03)
    return 0.34 * env * voice + 0.025 * env * smooth


def rabbit(time: float, duration: float, white: float, smooth: float) -> float:
    first = pulse(time, 0.08, 0.075)
    second = pulse(time, 0.24, 0.065)
    breath = (first + 0.72 * second) * (0.60 * smooth + 0.18 * white)
    chirp = (first * math.sin(TAU * 690.0 * time) +
             second * math.sin(TAU * 820.0 * time))
    return 0.23 * breath + 0.08 * chirp


def marsh_bird(time: float, duration: float, white: float, smooth: float) -> float:
    value = 0.0
    for index, centre in enumerate((0.08, 0.23, 0.39)):
        gate = pulse(time, centre, 0.065)
        local = time - centre + 0.065
        frequency = 980.0 + index * 155.0 + 520.0 * local
        value += gate * (math.sin(TAU * frequency * local) +
                         0.22 * math.sin(TAU * frequency * 2.0 * local))
    return 0.25 * value


def grass_step(time: float, duration: float, white: float, smooth: float) -> float:
    env = envelope(time, duration, 0.003, 0.12) * math.exp(-5.0 * time)
    return 0.54 * env * (0.62 * smooth + 0.38 * white)


def stone_step(time: float, duration: float, white: float, smooth: float) -> float:
    env = envelope(time, duration, 0.002, 0.10) * math.exp(-7.0 * time)
    knock = math.sin(TAU * 118.0 * time) + 0.38 * math.sin(TAU * 236.0 * time)
    return 0.48 * env * (0.66 * knock + 0.24 * white)


def wood_step(time: float, duration: float, white: float, smooth: float) -> float:
    env = envelope(time, duration, 0.002, 0.11) * math.exp(-6.0 * time)
    knock = math.sin(TAU * 154.0 * time) + 0.46 * math.sin(TAU * 308.0 * time)
    return 0.42 * env * (0.76 * knock + 0.16 * smooth)


def sand_step(time: float, duration: float, white: float, smooth: float) -> float:
    env = envelope(time, duration, 0.004, 0.14) * math.exp(-4.0 * time)
    scrape = 0.42 * white + 0.58 * smooth
    return 0.46 * env * scrape


SAMPLES: dict[str, tuple[float, int, Renderer]] = {
    "ambient-forest.wav": (2.70, 9101, forest),
    "ambient-river.wav": (2.70, 9102, river),
    "ambient-coast.wav": (2.70, 9103, coast),
    "animal-sheep.wav": (0.62, 9111, sheep),
    "animal-rabbit.wav": (0.38, 9112, rabbit),
    "animal-marsh-bird.wav": (0.52, 9113, marsh_bird),
    "footstep-grass-dirt-1.wav": (0.19, 9121, grass_step),
    "footstep-grass-dirt-2.wav": (0.21, 9122, grass_step),
    "footstep-stone-1.wav": (0.17, 9131, stone_step),
    "footstep-stone-2.wav": (0.19, 9132, stone_step),
    "footstep-wood-1.wav": (0.18, 9141, wood_step),
    "footstep-wood-2.wav": (0.20, 9142, wood_step),
    "footstep-sand-1.wav": (0.20, 9151, sand_step),
    "footstep-sand-2.wav": (0.22, 9152, sand_step),
}


def wave_bytes(duration: float, seed: int, renderer: Renderer) -> bytes:
    frame_count = round(duration * SAMPLE_RATE)
    state = seed & 0xFFFFFFFF
    smoothed = 0.0
    pcm = bytearray()
    for frame in range(frame_count):
        state = (state * 1_664_525 + 1_013_904_223) & 0xFFFFFFFF
        white = state / 2_147_483_647.5 - 1.0
        smoothed += (white - smoothed) * 0.085
        time = frame / SAMPLE_RATE
        value = max(-1.0, min(1.0, renderer(time, duration, white, smoothed)))
        sample = int(math.floor(value * 32767.0 + (0.5 if value >= 0.0 else -0.5)))
        pcm.extend(struct.pack("<h", max(-32768, min(32767, sample))))

    data_size = len(pcm)
    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF", 36 + data_size, b"WAVE", b"fmt ", 16, 1, 1,
        SAMPLE_RATE, SAMPLE_RATE * 2, 2, 16, b"data", data_size)
    return header + pcm


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output = args.root / "media" / "audio" / "samples"
    output.mkdir(parents=True, exist_ok=True)

    failed = False
    for name, (duration, seed, renderer) in SAMPLES.items():
        expected = wave_bytes(duration, seed, renderer)
        path = output / name
        digest = hashlib.sha256(expected).hexdigest()
        if args.check:
            actual = path.read_bytes() if path.is_file() else b""
            if actual != expected:
                print(f"[B9_AUDIO] mismatch={name} expected_sha256={digest}")
                failed = True
        else:
            path.write_bytes(expected)
            print(f"[B9_AUDIO] generated={name} bytes={len(expected)} sha256={digest}")

    if failed:
        return 1
    print(f"[B9_AUDIO] status=PASS mode={'check' if args.check else 'generate'} "
          f"samples={len(SAMPLES)} rate={SAMPLE_RATE} channels=1 bits=16")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
