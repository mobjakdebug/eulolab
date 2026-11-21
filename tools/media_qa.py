#!/usr/bin/env python3
"""Mechanical QA for host-derived release media; not a hardware test.

Decode all video frames, check the expected edit timing/audio, and compare
representative *decoded* product frames to this checkout's native C++ host.
A person still has to watch the films and review their claims before release.
"""
from __future__ import annotations

from pathlib import Path
import subprocess

import imageio_ffmpeg
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT = 1280, 720
TEASER = "video/eulolab-teaser.mp4"
REPLAY = "video/eulolab-host-replay.mp4"
POSTER = "docs/teaser-poster.png"


class MediaError(ValueError):
    """The rendered release media does not match the app/storyboard."""


def metadata(file: Path, *, fps: int, seconds: int, audio: bool) -> None:
    reader = imageio_ffmpeg.read_frames(str(file))
    try:
        info = next(reader)
    finally:
        reader.close()
    if (info.get("codec") != "h264" or info.get("source_size") != (WIDTH, HEIGHT)
            or abs(info.get("fps", 0) - fps) > 0.01
            or abs(info.get("duration", 0) - seconds) > 0.1
            or (info.get("audio_codec") != "aac" if audio else
                info.get("audio_codec") is not None)):
        raise MediaError(f"Wrong codec, format, duration or audio for {file}: {info}")
    frames, duration = imageio_ffmpeg.count_frames_and_secs(str(file))
    if frames != fps * seconds or abs(duration - seconds) > 0.1:
        raise MediaError(f"Incomplete video {file}: {frames} frames over {duration}s")
    if audio:
        # The frame-count pass decodes video, not audio. Explicitly decode AAC.
        command = [imageio_ffmpeg.get_ffmpeg_exe(), "-v", "error", "-xerror", "-i",
                   str(file), "-map", "0:a:0", "-f", "null", "-"]
        subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       timeout=60, check=True)


def decoded_frame(file: Path, time: float) -> np.ndarray:
    command = [imageio_ffmpeg.get_ffmpeg_exe(), "-hide_banner", "-loglevel", "error",
               "-ss", f"{time:.6f}", "-i", str(file), "-frames:v", "1", "-f",
               "rawvideo", "-pix_fmt", "rgb24", "pipe:1"]
    video = subprocess.run(command, capture_output=True, check=True, timeout=60).stdout
    if len(video) != WIDTH * HEIGHT * 3:
        raise MediaError(f"Could not decode frame at {time}s of {file}")
    return np.frombuffer(video, dtype=np.uint8).reshape(HEIGHT, WIDTH, 3)


def native_frame(request: str) -> np.ndarray:
    frame = subprocess.run([str(ROOT / "out/eulolab-host"), "--demo-times"],
                           input=(request + "\n").encode("ascii"), capture_output=True,
                           check=True, timeout=30).stdout
    if len(frame) != WIDTH * HEIGHT * 4:
        raise MediaError(f"Native app did not render {request!r}")
    # Native framebuffer stores AARRGGBB as BGRA bytes on little-endian hosts.
    return np.frombuffer(frame, dtype=np.uint8).reshape(HEIGHT, WIDTH, 4)[:, :, [2, 1, 0]]


def compare(decoded: np.ndarray, native: np.ndarray, label: str, max_error: float = 8) -> None:
    error = np.abs(decoded.astype(np.int16) - native.astype(np.int16)).mean()
    if error > max_error:
        raise MediaError(f"{label} differs from the current native app (RGB error {error:.2f})")
    print(f"  {label}: native comparison RGB error {error:.2f}")


def check_media(root: Path = ROOT) -> None:
    teaser, replay, poster = (root / name for name in (TEASER, REPLAY, POSTER))
    metadata(teaser, fps=24, seconds=30, audio=True)
    metadata(replay, fps=12, seconds=79, audio=False)
    with Image.open(poster) as image:
        if image.size != (WIDTH, HEIGHT) or image.format != "PNG":
            raise MediaError("Release poster has wrong format or dimensions")
        image.load()
        still = np.asarray(image.convert("RGB"))
    compare(decoded_frame(teaser, 29), still, "Poster / final card", max_error=5)
    # These are uncropped product frames in the timed edit, not a static older
    # Bench screenshot, made-up Ground reading, or editorial artwork.
    for time, request, name in (
            (7.0, "22", "Design / fin inspector"),
            (11.0, "75", "Ground / no data"),
            (17.0, f"{44 + 28.8 * (17 * 24 - 366) / (540 - 366 - 1):.6f}",
             "Flight / calculated model"),
            (25.0, "BENCH_SETUP", "Bench / disconnected profile"),
            (26.5, "BENCH", "Bench / disconnected manual")):
        compare(decoded_frame(teaser, time), native_frame(request), name)
    for time in (22, 50, 75):
        compare(decoded_frame(replay, time), native_frame(str(time)),
                f"Unedited host replay / {time}s")
    print("Media QA: 720 + 948 decoded frames, AAC, poster and eight native product frames")


if __name__ == "__main__":
    check_media()
