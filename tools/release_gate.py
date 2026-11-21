#!/usr/bin/env python3
"""Fail a release if README/media inputs are stale; never invent evidence.

`make release-media` rebuilds two host videos, poster, captions, then writes a
hash manifest of the *current* checkout. `make release-check BASE=<prior-tag>
TAG=<new-tag>` checks that manifest, requires an updated README and a per-tag
human review record. Tag and published-release CI both check, regenerate,
then retain fresh media. This is not a hardware certification.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "docs/release-media.json"
ASSETS = (
    "video/eulolab-teaser.mp4",
    "video/eulolab-host-replay.mp4",
    "video/eulolab-teaser.vtt",
    "docs/teaser-poster.png",
)
EXCLUDED = {*ASSETS, MANIFEST}


class ReleaseError(ValueError):
    """Release inputs must be corrected, not silently published."""


def git_files(root: Path) -> list[str]:
    # Include new files being prepared for the release, but not ignored build
    # products. Stable sorting makes the digest independent of git's order.
    output = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=root,
    )
    return sorted({path.decode("utf-8") for path in output.split(b"\0") if path})


def source_fingerprint(root: Path, paths: list[str]) -> tuple[str, int]:
    digest = hashlib.sha256()
    count = 0
    for relative in sorted(set(paths) - EXCLUDED):
        file = root / relative
        if not file.is_file():
            raise ReleaseError(f"Tracked source is missing: {relative}")
        digest.update(relative.encode("utf-8") + b"\0")
        digest.update(hashlib.sha256(file.read_bytes()).digest())
        count += 1
    if not count:
        raise ReleaseError("No release sources were found")
    return digest.hexdigest(), count


def asset_hashes(root: Path) -> dict[str, str]:
    assets = {}
    for relative in ASSETS:
        file = root / relative
        minimum = 1000 if relative.endswith(".vtt") else 20000
        if not file.is_file() or file.stat().st_size < minimum:
            raise ReleaseError(f"Missing or incomplete media: {relative}")
        assets[relative] = hashlib.sha256(file.read_bytes()).hexdigest()
    if b"ftyp" not in (root / ASSETS[0]).read_bytes()[:32]:
        raise ReleaseError("Walkthrough is not an MP4 container")
    if b"ftyp" not in (root / ASSETS[1]).read_bytes()[:32]:
        raise ReleaseError("Host replay is not an MP4 container")
    if not (root / ASSETS[3]).read_bytes().startswith(b"\x89PNG\r\n\x1a\n"):
        raise ReleaseError("Poster is not a PNG")
    return assets


def changed_since(root: Path, base: str, relative: str) -> None:
    try:
        subprocess.run(["git", "rev-parse", "--verify", "--quiet",
                        f"{base}^{{commit}}"], cwd=root, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["git", "merge-base", "--is-ancestor", base, "HEAD"],
                       cwd=root, check=True, stdout=subprocess.DEVNULL)
        result = subprocess.run(["git", "diff", "--quiet", base, "--", relative],
                                cwd=root, check=False).returncode
    except subprocess.CalledProcessError as exc:
        raise ReleaseError(f"Previous release base is unknown or not an ancestor: {base}") from exc
    if result == 0:
        raise ReleaseError(f"{relative} has not changed since {base}")
    if result != 1:
        raise ReleaseError(f"Cannot compare {relative} with {base}")


def check_readme(root: Path, base: str | None) -> None:
    readme = (root / "README.md").read_text(encoding="utf-8")
    required = ("# EuloLab", "video/eulolab-teaser.mp4", "make preview",
                "make release-media", "make release-check", "docs/release-review.md")
    if len(readme) < 700 or any(fragment not in readme for fragment in required):
        raise ReleaseError("README is missing the quickstart, media or release-review links")
    stale = ("Live sensors and LoRa: not implemented", "EULOLAB/2 project files")
    if any(fragment in readme for fragment in stale):
        raise ReleaseError("README still describes an obsolete release")
    if base:
        changed_since(root, base, "README.md")


def check_tag_review(root: Path, base: str | None, tag: str) -> None:
    if not base:
        raise ReleaseError("A release tag requires BASE=<previous-tag> (root commit for the first release)")
    review = (root / "docs/release-review.md").read_text(encoding="utf-8")
    if not re.search(r"(?m)^Release tag: " + re.escape(tag) + r"$", review):
        raise ReleaseError(f"docs/release-review.md must name the exact release tag {tag}")
    reviewer = re.search(r"(?m)^Reviewed by: (.+)$", review)
    if not reviewer or any(x in reviewer.group(1).lower() for x in
                           ("pending", "tbd", "todo", "agent", "placeholder")):
        raise ReleaseError("A human reviewer must sign docs/release-review.md")
    checks = [line for line in review.splitlines() if line.startswith("- [")]
    if len(checks) != 4 or any(not line.startswith("- [x] ") for line in checks):
        raise ReleaseError("The four release review checks must be completed")
    changed_since(root, base, "docs/release-review.md")


def check_captions(root: Path) -> None:
    # The storyboard and WebVTT must agree, not merely carry plausible timecodes.
    if __package__:
        from .make_teaser import render_captions
    else:
        from make_teaser import render_captions
    file = (root / "video/eulolab-teaser.vtt").read_text(encoding="utf-8")
    if file != render_captions():
        raise ReleaseError("Captions differ from the video storyboard; run make release-media")


def check_video_content(root: Path) -> None:
    if __package__:
        from .media_qa import check_media
    else:
        from media_qa import check_media
    check_media(root)


def record(root: Path) -> dict:
    source_hash, source_count = source_fingerprint(root, git_files(root))
    return {
        "schema": 1,
        "source_sha256": source_hash,
        "source_files": source_count,
        "assets": asset_hashes(root),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    operation = parser.add_mutually_exclusive_group(required=True)
    operation.add_argument("--write", action="store_true", help="Stamp freshly rebuilt media")
    operation.add_argument("--check", action="store_true", help="Verify media and README")
    parser.add_argument("--base", help="Previous release tag/commit; require README changes")
    parser.add_argument("--tag", help="Planned/current tag; require a signed per-release review")
    args = parser.parse_args(argv)
    try:
        if args.write and (args.base or args.tag):
            raise ReleaseError("Only --check accepts --base/--tag")
        base = (args.base or os.environ.get("RELEASE_BASE")) if args.check else None
        tag = (args.tag or os.environ.get("RELEASE_TAG")) if args.check else None
        check_readme(ROOT, base)
        if tag:
            check_tag_review(ROOT, base, tag)
        check_captions(ROOT)
        current = record(ROOT)
        path = ROOT / MANIFEST
        if args.write:
            check_video_content(ROOT)
            path.write_text(json.dumps(current, indent=2, sort_keys=True) + "\n",
                            encoding="utf-8")
            print(f"Stamped current README and {len(ASSETS)} media files in {MANIFEST}")
        else:
            if not path.is_file():
                raise ReleaseError(f"No {MANIFEST}; rebuild with make release-media")
            previous = json.loads(path.read_text(encoding="utf-8"))
            if not isinstance(previous, dict):
                raise ReleaseError("Media provenance must be a JSON object")
            if previous != current:
                changed = [key for key in current if previous.get(key) != current[key]]
                raise ReleaseError("Release provenance is stale (" + ", ".join(changed) +
                                   "); rewrite README and run make release-media")
            check_video_content(ROOT)
            readme_status = (f"README changed since {base}" if base else
                             "README present (no previous-tag comparison)")
            print(f"Release check: {readme_status}; storyboard, media and native frames verified")
            if not tag:
                print("Manual reviewer sign-off not checked: pass TAG=<planned-release-tag> to make")
        return 0
    except (OSError, ValueError, RuntimeError, ImportError, StopIteration,
            subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        print(f"Release check failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
