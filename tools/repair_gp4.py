#!/usr/bin/env python3
"""Make GP4's directory tree agree with its file entries before pkg_build.

Some OpenOrbis create-gp4 releases emit a directory listing inconsistent with
--files. LibOrbisPkg resolves each file against <rootdir> and aborts when a
directory is absent. Reconstruct ONLY the GP4 directory metadata from the
package's declared target paths; never add undeclared source files.
"""
from pathlib import Path, PurePosixPath
import sys
import xml.etree.ElementTree as ET


def repair(project: Path, stage: Path) -> int:
    tree = ET.parse(project)
    root = tree.getroot()
    if root.tag != "psproject":
        raise ValueError("Expected a GP4 psproject document")
    files = root.find("files")
    rootdir = root.find("rootdir")
    if files is None or rootdir is None:
        raise ValueError("GP4 must have <files> and <rootdir> elements")

    paths = set()
    directories: dict[str, dict] = {}
    for entry in files.findall("file"):
        target = entry.get("targ_path", "")
        path = PurePosixPath(target)
        if not target or path.is_absolute() or any(part in (".", "..") for part in target.split("/")):
            raise ValueError(f"Unsafe GP4 target path: {target!r}")
        if target in paths:
            raise ValueError(f"Duplicate GP4 target path: {target!r}")
        paths.add(target)
        if not (stage / target).is_file():
            raise ValueError(f"Missing staged file for GP4 target: {target!r}")
        branch = directories
        for part in path.parts[:-1]:
            branch = branch.setdefault(part, {})
    if not paths or "sce_sys" not in directories:
        raise ValueError("GP4 has no package files or no sce_sys directory")

    rootdir.clear()
    def add_dirs(parent: ET.Element, branches: dict) -> int:
        count = 0
        for name, children in sorted(branches.items()):
            element = ET.SubElement(parent, "dir", {"targ_name": name})
            count += 1 + add_dirs(element, children)
        return count

    count = add_dirs(rootdir, directories)
    ET.indent(tree, space="\t")
    tree.write(project, encoding="utf-8", xml_declaration=True)
    return count


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: python3 tools/repair_gp4.py GP4_PATH STAGE_DIR")
    try:
        size = repair(Path(sys.argv[1]), Path(sys.argv[2]))
    except (ValueError, OSError, ET.ParseError) as error:
        raise SystemExit(f"GP4 directory repair failed: {error}") from None
    print(f"Validated staged GP4 file list and rebuilt {size} directory entries")
