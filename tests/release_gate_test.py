"""Release gate's provenance checks must detect stale copy without media codecs."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.release_gate import (ASSETS, MANIFEST, ReleaseError, asset_hashes,
                                check_tag_review, source_fingerprint)  # noqa: E402


class ReleaseGateTest(unittest.TestCase):
    def test_source_changes_but_generated_media_is_excluded(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'README.md').write_text('# Current release\n')
            video = root / ASSETS[0]
            video.parent.mkdir(parents=True)
            video.write_bytes(b'media-1')
            paths = ['README.md', ASSETS[0], MANIFEST]
            original, count = source_fingerprint(root, paths)
            self.assertEqual(count, 1)
            video.write_bytes(b'media-2')
            self.assertEqual(source_fingerprint(root, paths), (original, count))
            (root / 'README.md').write_text('# Updated release\n')
            self.assertNotEqual(source_fingerprint(root, paths)[0], original)
            (root / 'README.md').unlink()
            with self.assertRaises(ReleaseError):
                source_fingerprint(root, paths)

    def test_manifest_rejects_missing_and_invalid_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ReleaseError, 'Missing or incomplete'):
                asset_hashes(root)
            for name in ASSETS:
                file = root / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b'x' * (20000 if not name.endswith('.vtt') else 1200))
            with self.assertRaisesRegex(ReleaseError, 'MP4'):
                asset_hashes(root)
            for name in ASSETS[:2]:
                file = root / name
                file.write_bytes(b'\x00\x00\x00\x18ftyp' + file.read_bytes()[8:])
            with self.assertRaisesRegex(ReleaseError, 'PNG'):
                asset_hashes(root)
            poster = root / ASSETS[3]
            poster.write_bytes(b'\x89PNG\r\n\x1a\n' + poster.read_bytes()[8:])
            self.assertEqual(len(asset_hashes(root)), 4)

    def test_release_tag_needs_fresh_human_review_attestation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            doc = root / 'docs/release-review.md'
            doc.parent.mkdir()
            candidate = ('Release tag: pending\nReviewed by: pending maintainer review\n' +
                         '- [ ] Checked the README\n' * 4)
            doc.write_text(candidate)
            with self.assertRaisesRegex(ReleaseError, 'BASE'):
                check_tag_review(root, None, 'v1')
            with self.assertRaisesRegex(ReleaseError, 'exact release tag'):
                check_tag_review(root, 'previous', 'v1')
            doc.write_text(candidate.replace('tag: pending', 'tag: v1'))
            with self.assertRaisesRegex(ReleaseError, 'human reviewer'):
                check_tag_review(root, 'previous', 'v1')
            doc.write_text(doc.read_text().replace('pending maintainer review',
                                                   'A. Reviewer'))
            with self.assertRaisesRegex(ReleaseError, 'four release review checks'):
                check_tag_review(root, 'previous', 'v1')
            doc.write_text(doc.read_text().replace('[ ]', '[x]'))
            with patch('tools.release_gate.changed_since') as changed:
                check_tag_review(root, 'previous', 'v1')
                changed.assert_called_once_with(root, 'previous', 'docs/release-review.md')


if __name__ == '__main__':
    unittest.main()
