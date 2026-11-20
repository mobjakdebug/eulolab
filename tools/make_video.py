#!/usr/bin/env python3
"""Record real pixels produced by the C++ App on a host, NOT by a PS4.

The deterministic scenario changes the real C++ Rocket model through the
host demo sequencer and displays the shared C++ renderer and physics, including
published commercial .eng curves. The program itself marks each frame HOST
REPLAY. This is not footage of Orbis hardware or a physical rocket launch.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "video" / "eulolab-host-replay.mp4"
FPS = 12
SECONDS = 79


def ffmpeg_path() -> str:
    executable = os.environ.get("FFMPEG") or shutil.which("ffmpeg")
    if executable:
        return executable
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        raise SystemExit("Install ffmpeg or: python3 -m pip install imageio-ffmpeg") from None


def main() -> int:
    OUT.parent.mkdir(exist_ok=True)
    command = [
        ffmpeg_path(), "-hide_banner", "-loglevel", "error", "-y",
        "-f", "rawvideo", "-pixel_format", "bgra", "-video_size", "1280x720",
        "-framerate", str(FPS), "-i", "pipe:0", "-an",
        "-c:v", "libx264", "-preset", "veryfast", "-crf", "27",
        "-pix_fmt", "yuv420p", "-movflags", "+faststart",
        "-metadata", "title=EuloLab - host replay of real C++ app",
        "-metadata", "comment=Scripted HOST capture; NOT a PS4 console capture or physical launch",
        str(OUT),
    ]
    host = subprocess.Popen(
        [str(ROOT / "out/eulolab-host"), "--demo-raw", str(FPS), str(SECONDS)],
        cwd=ROOT, stdout=subprocess.PIPE,
    )
    assert host.stdout is not None
    try:
        encoder = subprocess.Popen(command, cwd=ROOT, stdin=host.stdout)
        host.stdout.close()
        encoded = encoder.wait()
        rendered = host.wait()
        if rendered or encoded:
            OUT.unlink(missing_ok=True)
            print(f"Render failed: host={rendered}, ffmpeg={encoded}", file=sys.stderr)
            return 1
    except BaseException:
        host.kill()
        host.wait()
        OUT.unlink(missing_ok=True)
        raise
    print(f"Recorded {SECONDS}s x {FPS}fps from the actual host program: {OUT} ({OUT.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
