"""Exercise the package manifest correction without requiring the PS4 SDK."""
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.repair_gp4 import repair


class RepairGp4Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.stage = Path(self.temp.name)
        self.project = self.stage / "pkg.gp4"
        self.files = ("eboot.bin", "sce_sys/param.sfo", "sce_sys/about/right.sprx",
                      "assets/motors/Klima_C6.eng")
        for name in self.files:
            file = self.stage / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b"test")

    def write_manifest(self, paths=None):
        root = ET.Element("psproject", {"fmt": "gp4", "version": "1000"})
        ET.SubElement(root, "volume")
        entries = ET.SubElement(root, "files")
        for name in (paths if paths is not None else self.files):
            ET.SubElement(entries, "file", {"targ_path": name, "orig_path": name})
        wrong = ET.SubElement(root, "rootdir")
        ET.SubElement(wrong, "dir", {"targ_name": "obsolete"})
        ET.ElementTree(root).write(self.project, encoding="utf-8", xml_declaration=True)

    def test_rebuilds_from_declared_files(self):
        self.write_manifest()
        assert repair(self.project, self.stage) == 4
        root = ET.parse(self.project).getroot()
        self.assertEqual([item.get("targ_path") for item in root.findall("./files/file")], list(self.files))
        self.assertEqual([d.get("targ_name") for d in root.findall("./rootdir/dir")],
                         ["assets", "sce_sys"])
        self.assertEqual(root.find("./rootdir/dir[@targ_name='assets']/dir").get("targ_name"), "motors")
        self.assertEqual(root.find("./rootdir/dir[@targ_name='sce_sys']/dir").get("targ_name"), "about")
        self.assertIsNone(root.find("./rootdir/dir[@targ_name='obsolete']"))

    def test_rejects_missing_duplicate_and_unsafe_paths(self):
        for paths in (("sce_sys/param.sfo", "missing.dat"),
                      ("sce_sys/param.sfo", "sce_sys/param.sfo"),
                      ("sce_sys/param.sfo", "../secret")):
            with self.subTest(paths=paths):
                self.write_manifest(paths)
                with self.assertRaises(ValueError):
                    repair(self.project, self.stage)


if __name__ == "__main__":
    unittest.main()
