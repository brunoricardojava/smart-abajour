import tempfile
import unittest
from pathlib import Path

from scripts.generate_manifest import MAX_FIRMWARE_SIZE, build_manifest, read_version


class GenerateManifestTest(unittest.TestCase):
    def test_builds_release_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(b"firmware")
            manifest = build_manifest("1.2.3", "v1.2.3", "abcdef012345", firmware)

        self.assertEqual(manifest["version"], "1.2.3")
        self.assertEqual(manifest["build"], "abcdef0")
        self.assertEqual(manifest["size"], 8)
        self.assertEqual(len(manifest["sha256"]), 64)
        self.assertTrue(manifest["url"].endswith("/v1.2.3/firmware-mhetesp32minikit.bin"))

    def test_rejects_tag_that_does_not_match_version(self):
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(b"firmware")
            with self.assertRaisesRegex(ValueError, "does not match"):
                build_manifest("1.2.3", "v1.2.4", "abcdef0", firmware)

    def test_rejects_firmware_larger_than_ota_slot(self):
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(b"x" * (MAX_FIRMWARE_SIZE + 1))
            with self.assertRaisesRegex(ValueError, "exceeds OTA slot"):
                build_manifest("1.2.3", "v1.2.3", "abcdef0", firmware)

    def test_rejects_non_semantic_version(self):
        with tempfile.TemporaryDirectory() as directory:
            version_file = Path(directory) / "VERSION"
            version_file.write_text("1.2\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "invalid semantic version"):
                read_version(version_file)

    def test_rejects_version_component_larger_than_firmware_parser(self):
        with tempfile.TemporaryDirectory() as directory:
            version_file = Path(directory) / "VERSION"
            version_file.write_text("10000.0.0\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "invalid semantic version"):
                read_version(version_file)


if __name__ == "__main__":
    unittest.main()
