#!/usr/bin/env python3
"""Rebuild the current, captioned 30-second product walkthrough.

All product frames come from *this build's* C++ host binary, including the
Ground empty state and disconnected Bench. No pre-recorded bench screenshots,
fabricated measurements, motor audio, console or physical flight footage.
The score is non-diegetic and synthesized here. Run: make teaser
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import wave

try:
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:
    raise SystemExit("Teaser needs Pillow + numpy: python3 -m pip install Pillow numpy imageio-ffmpeg") from exc

ROOT = Path(__file__).resolve().parents[1]
VIDEO = ROOT / "video/eulolab-teaser.mp4"
POSTER = ROOT / "docs/teaser-poster.png"
CAPTIONS = ROOT / "video/eulolab-teaser.vtt"
HOST = ROOT / "out/eulolab-host"
W, H, FPS, LENGTH = 1280, 720, 24, 30
FRAME_BYTES = W * H * 4
INK = (39, 52, 59)
PAPER = (246, 248, 247)
WHITE = (250, 252, 251)
BLUE = (35, 91, 147)
MUTED_DARK = (199, 216, 221)
MUTED_LIGHT = (74, 90, 99)
FONT_FILE = ROOT / "assets/SpaceGrotesk.ttf"

# Exact frame boundaries match video/eulolab-teaser.vtt. No footage from
# a real PS4, board, airframe, launch or test fixture exists in the repo.
DESIGN_CARD = 60       # 2.5 s
DESIGN = 96            # 4 s
GROUND_CARD = 198      # 8.25 s
GROUND = 240           # 10 s
FLIGHT_CARD = 324      # 13.5 s
FLIGHT = 366           # 15.25 s
BENCH_CARD = 540       # 22.5 s
BENCH_SETUP = 576      # 24 s
BENCH_MANUAL = 624     # 26 s
END = 660              # 27.5 s
TOTAL = LENGTH * FPS

# The captions and the chapter timings come from this same storyboard.
CAPTION_SEGMENTS = (
    (0, DESIGN_CARD,
     "EuloLab. Design inputs and recorded files.",
     "Native C++ host capture, not PS4 footage. Synthesized instrumental score; no dialogue."),
    (DESIGN_CARD, DESIGN,
     "Design: edit geometry, log real fins.",
     "Design parameters and operator-entered fin readings are separate; this film contains no fin measurements."),
    (DESIGN, GROUND_CARD,
     "Native host workbench: body, nose and fin model fields respond to scripted illustrative edits.",
     "No physical rocket or measured fin dimensions appear."),
    (GROUND_CARD, GROUND,
     "Ground data: load a file. There is no bundled sensor recording.",
     "Pressure and IMU playback require an actual user-supplied CSV."),
    (GROUND, FLIGHT_CARD,
     "Native host Ground page in its empty state. No sample data, sensor connection or GPS.",
     "An Arduino ESP32/STM32 recorder and optional LoRa roles compile, but have not been board-tested."),
    (FLIGHT_CARD, FLIGHT,
     "Flight: model a path, not a launch.",
     "Open-loop predictions are not real flight data or a safety result."),
    (FLIGHT, BENCH_CARD,
     "Native host-rendered, time-compressed model trajectory and attitude, including a labeled crop.",
     "It is not live telemetry, PS4 gameplay or footage of a physical flight."),
    (BENCH_CARD, BENCH_SETUP,
     "USB bench is manual and separate from Flight. No board is attached.",
     "The host preview has no USB bridge and cannot move a servo."),
    (BENCH_SETUP, BENCH_MANUAL,
     "Current native host view: EuloBench/2 USB profile editor, no device connected.",
     "These are requested settings, not a measured pinout or servo motion."),
    (BENCH_MANUAL, END,
     "Current native host view: disconnected manual channels. No movement was observed.", ""),
    (END, TOTAL,
     "Use your own records. Verify on hardware.",
     "Arduino ESP32/STM32 sketch is compile-only; PS4, sensors, RF and servo have no physical test."),
)


def vtt_time(frame: int) -> str:
    millis = frame * 1000 // FPS
    return f"{millis // 3600000:02d}:{(millis // 60000) % 60:02d}:{(millis // 1000) % 60:02d}.{millis % 1000:03d}"


def render_captions() -> str:
    lines = ["WEBVTT", ""]
    previous = 0
    for start, end, first, second in CAPTION_SEGMENTS:
        if start != previous or end <= start or end > TOTAL:
            raise ValueError("Caption boundaries must cover the video exactly")
        lines.extend((f"{vtt_time(start)} --> {vtt_time(end)}", first))
        if second:
            lines.append(second)
        lines.append("")
        previous = end
    if previous != TOTAL:
        raise ValueError("Captions do not reach the end of the film")
    return "\n".join(lines).rstrip() + "\n"


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT_FILE), size)


FONTS = {n: font(n) for n in (16, 18, 19, 21, 22, 37, 41, 59, 106)}


def ffmpeg_path() -> str:
    exe = os.environ.get("FFMPEG") or shutil.which("ffmpeg")
    if exe:
        return exe
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        raise SystemExit("Install ffmpeg or: python3 -m pip install imageio-ffmpeg") from None


class NativeFrames:
    """Request native demo timestamps or real empty/disconnected UI states."""

    def __init__(self) -> None:
        self.proc = subprocess.Popen([str(HOST), "--demo-times"], cwd=ROOT,
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def frame(self, second: float | str) -> Image.Image:
        if self.proc.stdin is None or self.proc.stdout is None:
            raise RuntimeError("Native capture pipe closed")
        if isinstance(second, str):
            if second not in ("BENCH_SETUP", "BENCH"):
                raise ValueError("Only disconnected native Bench frames are available")
            request = second
        else:
            request = f"{second:.6f}"
        self.proc.stdin.write((request + "\n").encode("ascii"))
        self.proc.stdin.flush()
        buffer = bytearray(FRAME_BYTES)
        view = memoryview(buffer)
        received = 0
        while received < FRAME_BYTES:
            count = self.proc.stdout.readinto(view[received:])
            if not count:
                raise RuntimeError(f"Host frame truncated at {received}/{FRAME_BYTES} bytes")
            received += count
        # The little-endian C++ framebuffer is AARRGGBB, i.e. BGRA in memory.
        return Image.frombytes("RGB", (W, H), buffer, "raw", "BGRX")

    def close(self) -> None:
        if self.proc.stdin:
            self.proc.stdin.close()
        if self.proc.stdout:
            self.proc.stdout.close()
        if self.proc.wait(timeout=15):
            raise RuntimeError(f"Native capture exited with status {self.proc.returncode}")

    def abort(self) -> None:
        if self.proc.poll() is None:
            self.proc.kill()
        self.proc.wait()


def measure_rules(draw: ImageDraw.ImageDraw, x: int, y0: int, y1: int,
                  color: tuple[int, int, int]) -> None:
    """The model drawing's dimensional witness, not a physical measurement."""
    draw.line((x, y0, x, y1), fill=color, width=2)
    for y in (y0, y0 + 110, y0 + 220, y0 + 330, y1):
        draw.line((x - 6, y, x + 13, y), fill=color, width=2)


def draw_open(still: Image.Image, frame_no: int) -> Image.Image:
    image = Image.new("RGB", (W, H), INK)
    draw = ImageDraw.Draw(image)
    draw.line((70, 58, 1210, 58), fill=(99, 125, 136), width=1)
    draw.text((70, 79), "EULOLAB   /   ROCKET STUDIO", font=FONTS[18], fill=MUTED_DARK)
    draw.text((1040, 79), "HOST PREVIEW", font=FONTS[18], fill=MUTED_DARK)

    # A crop of the actual selected fin viewport, not an illustration or a logo.
    # Stop before the native dimension label at x>710, rather than clipping a
    # lone letter at the right edge of the editorial assembly crop.
    crop = still.crop((483, 148, 710, 556)).resize((290, 500), Image.Resampling.LANCZOS)
    x = 851 + round(12 * max(0, 1 - frame_no / 16))
    image.paste(crop, (x, 120))
    draw.rectangle((x - 1, 119, x + 290, 620), outline=MUTED_DARK, width=1)
    draw.text((x, 628), "ASSEMBLY  /  ACTUAL HOST RENDER", font=FONTS[16], fill=MUTED_DARK)

    draw.text((66, 175), "EuloLab", font=FONTS[106], fill=WHITE)
    draw.line((72, 322, 365, 322), fill=(119, 172, 217), width=4)
    draw.text((71, 362), "Design inputs.", font=FONTS[41], fill=WHITE)
    draw.text((71, 418), "Recorded files.", font=FONTS[41], fill=WHITE)
    draw.text((72, 611), "NATIVE C++ WORKBENCH  /  HOST CAPTURE", font=FONTS[19],
              fill=MUTED_DARK)
    draw.text((72, 645), "NO BOARD OR PS4 FOOTAGE", font=FONTS[18], fill=MUTED_DARK)
    return image


def draw_chapter(frame_no: int, start: int, number: str, section: str,
                 first: str, second: str, detail: str) -> Image.Image:
    image = Image.new("RGB", (W, H), PAPER)
    draw = ImageDraw.Draw(image)
    draw.line((70, 78, 1210, 78), fill=(201, 211, 214), width=1)
    draw.text((70, 100), "EULOLAB   /   ROCKET STUDIO", font=FONTS[18], fill=MUTED_LIGHT)
    draw.text((70, 207), number, font=FONTS[106], fill=BLUE)
    draw.text((223, 174), section, font=FONTS[21], fill=BLUE)
    draw.text((220, 243), first, font=FONTS[59], fill=INK)
    draw.text((220, 322), second, font=FONTS[59], fill=INK)
    draw.text((222, 475), detail, font=FONTS[22], fill=MUTED_LIGHT)
    # A datum is revealed once per chapter, not a perpetual looping animation.
    y0 = 183
    y1 = y0 + round(350 * min(1, (frame_no - start + 1) / 9))
    draw.line((1160, y0, 1160, y1), fill=BLUE, width=2)
    if y1 >= 533:
        measure_rules(draw, 1160, 183, 533, BLUE)
    draw.line((70, 598, 1210, 598), fill=(201, 211, 214), width=1)
    draw.text((70, 625), "C++ HOST PREVIEW  /  NO PS4 OR HARDWARE FOOTAGE",
              font=FONTS[18], fill=MUTED_LIGHT)
    return image


def draw_flight_detail(frame: Image.Image) -> Image.Image:
    """A labeled crop of real flight pixels makes the model view readable."""
    image = Image.new("RGB", (W, H), INK)
    image.paste(frame.crop((244, 146, 959, 558)).resize(
        (1142, 658), Image.Resampling.LANCZOS), (69, 51))
    draw = ImageDraw.Draw(image)
    draw.text((69, 12), "DETAIL CROP  /  NATIVE C++ FLIGHT VIEW", font=FONTS[18],
              fill=MUTED_DARK)
    draw.text((959, 12), "MODEL ONLY  /  NOT PS4", font=FONTS[18],
              fill=MUTED_DARK)
    return image


def draw_end(flight: Image.Image) -> Image.Image:
    image = Image.new("RGB", (W, H), INK)
    draw = ImageDraw.Draw(image)
    draw.line((70, 65, 1210, 65), fill=(99, 125, 136), width=1)
    draw.text((70, 88), "EULOLAB   /   HOST WORKBENCH", font=FONTS[18],
              fill=MUTED_DARK)
    draw.text((70, 167), "EuloLab", font=FONTS[106], fill=WHITE)
    draw.line((72, 318, 365, 318), fill=(119, 172, 217), width=4)
    draw.text((72, 354), "Use your own records.", font=FONTS[37], fill=WHITE)
    draw.text((72, 408), "Verify on hardware.", font=FONTS[37], fill=WHITE)
    draw.text((72, 542), "ESP32/STM32 Arduino sketch: compiled, not flashed.",
              font=FONTS[21], fill=MUTED_DARK)
    draw.text((72, 573), "PS4, sensors, RF and servo: no physical test.",
              font=FONTS[21], fill=MUTED_DARK)
    draw.line((70, 626, 1210, 626), fill=(99, 125, 136), width=1)
    draw.text((70, 647), "github.com/mobjakdebug/eulolab", font=FONTS[19],
              fill=MUTED_DARK)
    draw.text((855, 159), "CALCULATED PATH  /  HOST", font=FONTS[16],
              fill=MUTED_DARK)
    crop = flight.crop((246, 148, 672, 556)).resize((331, 344),
                                                   Image.Resampling.LANCZOS)
    image.paste(crop, (855, 196))
    draw.rectangle((854, 195, 1186, 540), outline=MUTED_DARK, width=1)
    return image


def make_score(path: Path) -> None:
    """Original restrained harmonic bed + edit cues, never diegetic equipment audio."""
    sample_rate = 48000
    count = LENGTH * sample_rate
    mix = np.zeros(count, dtype=np.float32)
    # The harmonic change marks the editorial acts; attack/release overlap gently.
    chords = (
        (0.0, 9.2, (110.0, 164.81, 220.0, 261.63)),
        (8.2, 16.0, (98.0, 146.83, 196.0, 246.94)),
        (15.0, 23.5, (82.41, 123.47, 164.81, 220.0)),
        (22.5, 30.0, (110.0, 164.81, 220.0, 293.66)),
    )
    for start, stop, notes in chords:
        a, b = int(start * sample_rate), int(stop * sample_rate)
        t = np.arange(b - a, dtype=np.float32) / sample_rate
        attack = np.minimum(1, t / 0.8)
        release = np.minimum(1, (stop - start - t) / 1.25)
        env = np.clip(np.minimum(attack, release), 0, 1)
        for i, note in enumerate(notes):
            volume = (0.019 if i == 0 else 0.011) * env
            phase = 2 * np.pi * note * t
            mix[a:b] += volume * (np.sin(phase) + 0.13 * np.sin(2 * phase))
    # Soft, short edit markers: these are score accents, not fake UI or servo sounds.
    for idx, frame in enumerate((0, DESIGN_CARD, DESIGN, GROUND_CARD,
                                 GROUND, FLIGHT_CARD, FLIGHT, BENCH_CARD,
                                 BENCH_SETUP, BENCH_MANUAL, END)):
        a = int(frame / FPS * sample_rate)
        length = int(0.16 * sample_rate)
        t = np.arange(length, dtype=np.float32) / sample_rate
        pulse = np.exp(-t * (31 if idx in (3, 4, 9) else 23))
        mix[a:a + length] += (0.040 if idx in (0, 6, 10) else 0.024) * pulse * (
            np.sin(2 * np.pi * (190 * t - 65 * t * t)) +
            0.24 * np.sin(2 * np.pi * 510 * t))
    fade = int(1.2 * sample_rate)
    mix[-fade:] *= np.linspace(1, 0, fade, dtype=np.float32)
    # Aim for a comfortably audible ambient cue without limiting or clipping.
    mix *= 5
    if np.max(np.abs(mix)) >= 0.9:
        raise ValueError("Score exceeds headroom")
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(sample_rate)
        out.writeframes((mix * 32767).astype("<i2").tobytes())


def main() -> int:
    if not HOST.is_file():
        raise SystemExit("Build the C++ host first: make all")
    native = NativeFrames()
    encoder = None
    temp = ROOT / "out/eulolab-teaser.mp4"
    temp_poster = ROOT / "out/teaser-poster.png"
    temp_captions = ROOT / "out/eulolab-teaser.vtt"
    try:
        temp_captions.write_text(render_captions(), encoding="utf-8")
        body, nose, fins = (native.frame(t) for t in (8, 16, 22))
        ground = native.frame(75)  # actual no-recording state, not mock telemetry
        setup = native.frame("BENCH_SETUP")  # current host UI; no USB bridge
        manual = native.frame("BENCH")
        path = native.frame(60)
        final = draw_end(path)
        final.save(temp_poster, optimize=True)

        audio = ROOT / "out/teaser-audio.wav"
        make_score(audio)
        command = [ffmpeg_path(), "-hide_banner", "-loglevel", "error", "-y",
                   "-f", "rawvideo", "-pixel_format", "rgb24", "-video_size",
                   f"{W}x{H}", "-framerate", str(FPS), "-i", "pipe:0",
                   "-i", str(audio), "-c:v", "libx264", "-preset", "medium",
                   "-crf", "19", "-pix_fmt", "yuv420p", "-c:a", "aac",
                   "-b:a", "160k", "-ar", "48000", "-t", str(LENGTH),
                   "-movflags", "+faststart", "-metadata",
                   "title=EuloLab | Recorded files and model | Host walkthrough", "-metadata",
                   "comment=Native host UI only. No actual sensor, RF, servo, launch or PS4 footage.",
                   str(temp)]
        encoder = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.PIPE)
        assert encoder.stdin is not None
        for frame_no in range(TOTAL):
            if frame_no < DESIGN_CARD:
                image = draw_open(fins, frame_no)
            elif frame_no < DESIGN:
                image = draw_chapter(frame_no, DESIGN_CARD, "01", "DESIGN  /  INPUTS",
                                     "Edit geometry.", "Log real fins.",
                                     "Design values and operator logs are separate")
            elif frame_no < GROUND_CARD:
                image = (body if frame_no < 132 else nose if frame_no < 162 else fins)
            elif frame_no < GROUND:
                image = draw_chapter(frame_no, GROUND_CARD, "02", "GROUND  /  FILES",
                                     "Load a file.", "No sample data.",
                                     "Pressure / IMU CSV needs an actual recording")
            elif frame_no < FLIGHT_CARD:
                image = ground
            elif frame_no < FLIGHT:
                image = draw_chapter(frame_no, FLIGHT_CARD, "03", "FLIGHT  /  MODEL",
                                     "Model a path.", "Not a launch.",
                                     "Open-loop predictions / not a safety result")
            elif frame_no < BENCH_CARD:
                # The C++ model maps film time to its *calculated* flight timeline.
                model_second = 44 + 28.8 * (frame_no - FLIGHT) / (BENCH_CARD - FLIGHT - 1)
                native_flight = native.frame(model_second)
                image = (draw_flight_detail(native_flight)
                         if FLIGHT + 48 <= frame_no < FLIGHT + 112 else native_flight)
            elif frame_no < BENCH_SETUP:
                image = draw_chapter(frame_no, BENCH_CARD, "04", "USB BENCH  /  SEPARATE",
                                     "Manual USB only.", "No board attached.",
                                     "Host has no USB bridge / no measured travel")
            elif frame_no < BENCH_MANUAL:
                image = setup
            elif frame_no < END:
                image = manual
            else:
                image = final
            encoder.stdin.write(image.tobytes())
            if frame_no % FPS == 0:
                print(f"Encoded timeline {frame_no // FPS:02d}/{LENGTH}s", flush=True)
        encoder.stdin.close()
        encoder.stdin = None
        if encoder.wait() != 0:
            raise RuntimeError("ffmpeg failed to encode the teaser")
        native.close()
        VIDEO.parent.mkdir(exist_ok=True)
        POSTER.parent.mkdir(exist_ok=True)
        temp.replace(VIDEO)
        temp_poster.replace(POSTER)
        temp_captions.replace(CAPTIONS)
        print(f"Walkthrough: {VIDEO} ({VIDEO.stat().st_size:,} bytes, {LENGTH}s at {FPS} fps)")
        print(f"Poster: {POSTER}; captions: {CAPTIONS}")
        return 0
    except BaseException:
        if encoder and encoder.poll() is None:
            encoder.kill()
            encoder.wait()
        native.abort()
        temp.unlink(missing_ok=True)
        temp_poster.unlink(missing_ok=True)
        temp_captions.unlink(missing_ok=True)
        raise


if __name__ == "__main__":
    raise SystemExit(main())
