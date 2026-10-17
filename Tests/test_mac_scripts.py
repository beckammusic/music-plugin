#!/usr/bin/env python3
"""Filesystem/command-contract tests; no Apple tools or Mach-O loading occurs.

Run: python3 Tests/test_mac_scripts.py
These tests intentionally mock macOS commands and cannot establish AU/Logic
compatibility, real codesigning, notarization, or a successful macOS build.
"""

import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile


PROJECT = Path(__file__).resolve().parents[1]
SCRIPTS = PROJECT / "Scripts"


class MacScriptTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="gummy-mac-script-tests-")
        self.root = Path(self.scratch.name)
        self.home = self.root / "user with spaces"
        self.home.mkdir()
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.build = self.root / "build with spaces"
        self.log = self.root / "commands.jsonl"
        self.script_dir = SCRIPTS
        self.env = dict(os.environ, HOME=str(self.home),
                        PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                        GUMMY_TEST_LOG=str(self.log),
                        GUMMY_TEST_BUILD=str(self.build),
                        GUMMY_PLIST_BUDDY=str(self.bin / "PlistBuddy"))
        self.env.pop("GUMMY_JUCE_PATH", None)
        self.mock("uname", "print('Darwin' if '-s' in sys.argv else 'arm64')")
        self.mock("id", "print('501')")
        self.mock("PlistBuddy", """
try:
    with open(sys.argv[3], 'rb') as f:
        value = plistlib.load(f)
    for part in sys.argv[2].removeprefix('Print :').split(':'):
        value = value[int(part)] if isinstance(value, list) else value[part]
    print(value)
except (OSError, KeyError, IndexError, ValueError):
    sys.exit(1)
""")
        self.mock("ditto", """
if os.environ.get('GUMMY_TEST_COPY_FAIL') == '1':
    sys.exit(9)
if '-c' in sys.argv:
    if os.environ.get('GUMMY_TEST_ARCHIVE_EXIT'):
        sys.exit(int(os.environ['GUMMY_TEST_ARCHIVE_EXIT']))
    import zipfile
    source = pathlib.Path(sys.argv[-2])
    with zipfile.ZipFile(sys.argv[-1], 'w') as archive:
        for item in source.rglob('*'):
            if item.is_file(): archive.write(item, pathlib.Path(source.name) / item.relative_to(source))
else:
    shutil.copytree(sys.argv[1], sys.argv[2], symlinks=True, copy_function=shutil.copy2)
""")
        self.mock("shasum", "import hashlib; print(hashlib.sha256(pathlib.Path(sys.argv[-1]).read_bytes()).hexdigest() + '  ' + sys.argv[-1])")
        self.mock("codesign", "sys.exit(int(os.environ.get('GUMMY_TEST_SIGN_EXIT', '0')))")
        self.mock("ctest", "sys.exit(int(os.environ.get('GUMMY_TEST_CTEST_EXIT', '0')))")
        self.mock("sleep", "pass")
        self.mock("auval", """
if sys.argv[1:] == ['-a']:
    enumeration_exit = int(os.environ.get('GUMMY_TEST_AU_ENUM_EXIT', '0'))
    if enumeration_exit: sys.exit(enumeration_exit)
    with open(os.environ['GUMMY_TEST_LOG']) as f:
        attempts = sum(json.loads(line) == ['auval', '-a'] for line in f)
    if attempts <= int(os.environ.get('GUMMY_TEST_DISCOVERY_DELAY', '0')):
        print('aufx othr Manu - Other Product')
    else:
        print(os.environ.get('GUMMY_TEST_AU_LIST', 'aufx Gmmy GmAu - GUMMY Audio: GUMMY'))
else:
    sys.exit(int(os.environ.get('GUMMY_TEST_AUVAL_EXIT', '0')))
""")
        for name in ("xcode-select", "xcrun"):
            self.mock(name, "print('/Applications/Xcode.app/Contents/Developer')")
        self.mock("cmake", """
if '--build' in sys.argv:
    code = int(os.environ.get('GUMMY_TEST_BUILD_EXIT', '0'))
    if code:
        sys.exit(code)
    root = pathlib.Path(os.environ['GUMMY_TEST_BUILD']) / 'GUMMY_artefacts/Release'
    for product in ('AU/GUMMY.component', 'VST3/GUMMY.vst3', 'Standalone/GUMMY.app'):
        (root / product).mkdir(parents=True, exist_ok=True)
""")

    def tearDown(self):
        self.scratch.cleanup()

    def mock(self, name, source):
        path = self.bin / name
        path.write_text("#!" + sys.executable + "\n"
                        "import os, sys, json, pathlib, plistlib, shutil\n"
                        "with open(os.environ['GUMMY_TEST_LOG'], 'a') as f:\n"
                        "    f.write(json.dumps([pathlib.Path(sys.argv[0]).name] + sys.argv[1:]) + '\\n')\n"
                        + source + "\n")
        path.chmod(0o755)

    def run_script(self, name, *args, input_text=None):
        return subprocess.run([str(self.script_dir / name), *map(str, args)], env=self.env,
                              input=input_text, text=True, capture_output=True)

    def bundle(self, destination, format_name, unrelated=False, marker="current"):
        destination = Path(destination)
        binary = destination / "Contents/MacOS/GUMMY"
        binary.parent.mkdir(parents=True)
        binary.write_text(marker)
        binary.chmod(0o755)
        info = {"CFBundleIdentifier": "org.unrelated.plugin" if unrelated else "com.gummyaudio.gummy",
                "CFBundleName": "GUMMY", "CFBundleExecutable": "GUMMY"}
        if format_name == "AU":
            info["AudioComponents"] = [{"type": "aufx", "subtype": "Gmmy", "manufacturer": "GmAu"}]
        with (destination / "Contents/Info.plist").open("wb") as f:
            plistlib.dump(info, f)
        resource = destination / "Contents/Resources/preserved resource.txt"
        resource.parent.mkdir()
        resource.write_text("bundle resource")
        return destination

    def source_bundles(self):
        root = self.build / "GUMMY_artefacts/Release"
        return (self.bundle(root / "AU/GUMMY.component", "AU"),
                self.bundle(root / "VST3/GUMMY.vst3", "VST3"))

    def installed_paths(self):
        root = self.home / "Library/Audio/Plug-Ins"
        return root / "Components/GUMMY.component", root / "VST3/GUMMY.vst3"

    def commands(self, name):
        if not self.log.exists():
            return []
        return [entry for line in self.log.read_text().splitlines()
                if (entry := json.loads(line))[0] == name]

    def test_shell_syntax(self):
        for script in sorted(SCRIPTS.glob("*.sh")):
            with self.subTest(script=script.name):
                result = subprocess.run(["bash", "-n", str(script)], capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_non_mac_is_rejected_without_installation(self):
        self.mock("uname", "print('Linux')")
        result = self.run_script("install_mac.sh", "--build-dir", self.build)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.home / "Library").exists())

    def test_install_preserves_complete_bundles_and_previous_version(self):
        self.source_bundles()
        au, vst = self.installed_paths()
        self.bundle(au, "AU", marker="old")
        result = self.run_script("install_mac.sh", "--build-dir", self.build)
        self.assertEqual(result.returncode, 0, result.stderr)
        for bundle in (au, vst):
            self.assertEqual((bundle / "Contents/MacOS/GUMMY").read_text(), "current")
            self.assertTrue(os.access(bundle / "Contents/MacOS/GUMMY", os.X_OK))
            self.assertEqual((bundle / "Contents/Resources/preserved resource.txt").read_text(), "bundle resource")
        backups = list(au.parent.glob(".GUMMY-backups/replaced.*/GUMMY.component/Contents/MacOS/GUMMY"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_text(), "old")

    def test_unrelated_installed_bundle_is_preserved(self):
        self.source_bundles()
        au, vst = self.installed_paths()
        self.bundle(au, "AU", unrelated=True, marker="unrelated")
        result = self.run_script("install_mac.sh", "--build-dir", self.build)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((au / "Contents/MacOS/GUMMY").read_text(), "unrelated")
        self.assertFalse(vst.exists())

    def test_missing_second_source_stops_before_install(self):
        self.bundle(self.build / "GUMMY_artefacts/Release/AU/GUMMY.component", "AU")
        result = self.run_script("install_mac.sh", "--build-dir", self.build)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.installed_paths()[0].exists())

    def test_copy_failure_preserves_previous_bundle(self):
        self.source_bundles()
        au, _ = self.installed_paths()
        self.bundle(au, "AU", marker="old")
        self.env["GUMMY_TEST_COPY_FAIL"] = "1"
        result = self.run_script("install_mac.sh", "--build-dir", self.build)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((au / "Contents/MacOS/GUMMY").read_text(), "old")

    def test_symbolic_link_target_is_refused(self):
        source_au, _ = self.source_bundles()
        au, _ = self.installed_paths()
        au.parent.mkdir(parents=True)
        au.symlink_to(source_au, target_is_directory=True)
        result = self.run_script("install_mac.sh", "--build-dir", self.build)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(au.is_symlink())
        self.assertTrue(source_au.exists())

    def test_symbolic_link_library_is_refused_before_creating_directories(self):
        self.source_bundles()
        outside = self.root / "outside user Library"
        outside.mkdir()
        (self.home / "Library").symlink_to(outside, target_is_directory=True)
        result = self.run_script("install_mac.sh", "--build-dir", self.build)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(list(outside.iterdir()), [])

    def test_uninstall_confirmation_and_preset_preservation(self):
        for bundle, format_name in zip(self.installed_paths(), ("AU", "VST3")):
            self.bundle(bundle, format_name)
        preset = self.home / "Library/Application Support/GUMMY/Presets/mine.gummy"
        preset.parent.mkdir(parents=True)
        preset.write_text("user preset")
        cancelled = self.run_script("uninstall_mac.sh", input_text="no\n")
        self.assertEqual(cancelled.returncode, 0, cancelled.stderr)
        self.assertTrue(all(bundle.exists() for bundle in self.installed_paths()))
        removed = self.run_script("uninstall_mac.sh", input_text="REMOVE\n")
        self.assertEqual(removed.returncode, 0, removed.stderr)
        self.assertFalse(any(bundle.exists() for bundle in self.installed_paths()))
        self.assertEqual(preset.read_text(), "user preset")

    def test_uninstall_refuses_unrelated_product(self):
        au, _ = self.installed_paths()
        self.bundle(au, "AU", unrelated=True)
        result = self.run_script("uninstall_mac.sh", input_text="REMOVE\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(au.exists())

    def test_validation_reads_metadata_and_invokes_optional_pluginval(self):
        au = self.bundle(self.installed_paths()[0], "AU")
        self.mock("pluginval", "pass")
        result = self.run_script("validate_au.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.commands("auval"), [["auval", "-a"], ["auval", "-v", "aufx", "Gmmy", "GmAu"]])
        self.assertEqual(self.commands("pluginval"), [["pluginval", "--strictness-level", "5", "--validate", str(au)]])

    def test_unregistered_au_and_failed_validation_are_reported(self):
        self.bundle(self.installed_paths()[0], "AU")
        self.env["GUMMY_TEST_AU_LIST"] = "aufx othr Manu - Other Product"
        undiscovered = self.run_script("validate_au.sh")
        self.assertNotEqual(undiscovered.returncode, 0)
        self.assertEqual(self.commands("auval"), [["auval", "-a"]] * 15)
        self.assertEqual(len(self.commands("sleep")), 14)
        self.assertIn("Other Product", undiscovered.stderr)
        self.assertNotIn("validation passed", undiscovered.stdout)
        self.env.pop("GUMMY_TEST_AU_LIST")
        self.env["GUMMY_TEST_AUVAL_EXIT"] = "1"
        failed = self.run_script("validate_au.sh")
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn("Audio Unit validation failed", failed.stderr)

    def test_delayed_registration_is_retried_before_actual_validation(self):
        self.bundle(self.installed_paths()[0], "AU")
        self.env["GUMMY_TEST_DISCOVERY_DELAY"] = "2"
        result = self.run_script("validate_au.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.commands("auval"), [["auval", "-a"]] * 3 +
                         [["auval", "-v", "aufx", "Gmmy", "GmAu"]])
        self.assertEqual(self.commands("sleep"), [["sleep", "2"]] * 2)
        self.assertIn("Audio Unit discovery and auval validation passed", result.stdout)

    def test_registration_on_last_attempt_still_runs_real_validation(self):
        self.bundle(self.installed_paths()[0], "AU")
        self.env["GUMMY_TEST_DISCOVERY_DELAY"] = "14"
        result = self.run_script("validate_au.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.commands("auval"), [["auval", "-a"]] * 15 +
                         [["auval", "-v", "aufx", "Gmmy", "GmAu"]])
        self.assertEqual(len(self.commands("sleep")), 14)

    def test_registry_enumeration_error_is_not_hidden_by_retries(self):
        self.bundle(self.installed_paths()[0], "AU")
        self.env["GUMMY_TEST_AU_ENUM_EXIT"] = "13"
        result = self.run_script("validate_au.sh")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("auval could not enumerate", result.stderr)
        self.assertEqual(self.commands("auval"), [["auval", "-a"]])
        self.assertEqual(self.commands("sleep"), [])

    def test_build_failure_is_propagated_and_universal_architecture_is_forwarded(self):
        juce = self.root / "JUCE"
        juce.mkdir()
        (juce / "CMakeLists.txt").write_text("project(JUCE VERSION 8.0.6 LANGUAGES C CXX)\n")
        self.env["GUMMY_TEST_BUILD_EXIT"] = "42"
        result = self.run_script("build_mac.sh", "--juce", juce, "--build-dir", self.build,
                                 "--arch", "universal", "--jobs", "2")
        self.assertEqual(result.returncode, 42, result.stderr)
        self.assertNotIn("Build succeeded", result.stdout)
        self.assertIn("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64", self.commands("cmake")[0])
        self.assertEqual(self.commands("ctest"), [])
        self.assertEqual(self.commands("codesign"), [])

    def test_successful_mock_build_checks_all_three_product_signatures(self):
        juce = self.root / "JUCE"
        juce.mkdir()
        (juce / "CMakeLists.txt").write_text("project(JUCE VERSION 8.0.6 LANGUAGES C CXX)\n")
        result = self.run_script("build_mac.sh", "--juce", juce, "--build-dir", self.build, "--jobs", "2")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Build succeeded", result.stdout)
        configure, build = self.commands("cmake")
        self.assertIn("-DGUMMY_BUILD_PLUGIN=ON", configure)
        self.assertIn("-DGUMMY_BUILD_TESTS=ON", configure)
        for target in ("GUMMY_AU", "GUMMY_VST3", "GUMMY_Standalone", "GummyDSPTests", "GummyPluginTests", "GummyFormatTests"):
            self.assertIn(target, build)
        self.assertEqual(self.commands("ctest"), [["ctest", "--test-dir", str(self.build), "-C", "Release", "--output-on-failure"]])
        command_names = [json.loads(line)[0] for line in self.log.read_text().splitlines()]
        self.assertLess(command_names.index("ctest"), command_names.index("codesign"))
        self.assertEqual(len(self.commands("codesign")), 6)

    def test_ctest_failure_prevents_signing_and_success_message(self):
        juce = self.root / "JUCE"
        juce.mkdir()
        (juce / "CMakeLists.txt").write_text("project(JUCE VERSION 8.0.6 LANGUAGES C CXX)\n")
        self.env["GUMMY_TEST_CTEST_EXIT"] = "8"
        result = self.run_script("build_mac.sh", "--juce", juce, "--build-dir", self.build, "--jobs", "2")
        self.assertEqual(result.returncode, 8, result.stderr)
        self.assertNotIn("Build succeeded", result.stdout)
        self.assertEqual(len(self.commands("ctest")), 1)
        self.assertEqual(self.commands("codesign"), [])

    def test_absent_local_juce_uses_cmake_pinned_fetch(self):
        # Isolate discovery from the real development checkout beside PROJECT.
        self.script_dir = self.root / "isolated project/Scripts"
        shutil.copytree(SCRIPTS, self.script_dir)
        result = self.run_script("build_mac.sh", "--build-dir", self.build, "--jobs", "2")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CMake will fetch pinned JUCE 8.0.6", result.stdout)
        self.assertIn("-DGUMMY_JUCE_PATH=", self.commands("cmake")[0])
        self.assertEqual(len(self.commands("ctest")), 1)

    def test_explicit_wrong_juce_version_stops_before_configuration(self):
        juce = self.root / "JUCE"
        juce.mkdir()
        (juce / "CMakeLists.txt").write_text("project(JUCE VERSION 8.0.5 LANGUAGES C CXX)\n")
        result = self.run_script("build_mac.sh", "--juce", juce, "--build-dir", self.build)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Expected pinned JUCE 8.0.6", result.stderr)
        self.assertEqual(self.commands("cmake"), [])
        self.assertEqual(self.commands("ctest"), [])


    def packaging_sources(self):
        self.source_bundles()
        self.bundle(self.build / "GUMMY_artefacts/Release/Standalone/GUMMY.app", "Standalone")
        return self.root / "downloads with spaces"

    def test_package_keeps_bundles_licenses_and_executable_launchers(self):
        output = self.packaging_sources()
        result = self.run_script("package_mac.sh", "--build-dir", self.build, "--output-dir", output)
        self.assertEqual(result.returncode, 0, result.stderr)
        archive = output / "GUMMY-macOS.zip"
        self.assertTrue((output / "GUMMY-macOS.zip.sha256").read_text().endswith("  GUMMY-macOS.zip\n"))
        with zipfile.ZipFile(archive) as z:
            self.assertIsNone(z.testzip())
            for relative in ("AU/GUMMY.component", "VST3/GUMMY.vst3", "Standalone/GUMMY.app"):
                name = "GUMMY-macOS/build-mac/GUMMY_artefacts/Release/" + relative + "/Contents/MacOS/GUMMY"
                self.assertEqual(z.read(name), b"current")
                self.assertNotEqual((z.getinfo(name).external_attr >> 16) & 0o111, 0)
            for launcher in ("Install", "Uninstall", "Validate"):
                name = "GUMMY-macOS/" + launcher + " GUMMY.command"
                self.assertNotEqual((z.getinfo(name).external_attr >> 16) & 0o111, 0)
            self.assertIn("GUMMY-macOS/Documentation/ThirdParty/JUCE-LICENSE.md", z.namelist())
            self.assertIn("GUMMY-macOS/START HERE.txt", z.namelist())
        self.assertEqual(len(self.commands("codesign")), 3)

    def test_package_missing_standalone_is_rejected(self):
        self.source_bundles()
        output = self.root / "download"
        result = self.run_script("package_mac.sh", "--build-dir", self.build, "--output-dir", output)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(output.exists())

    def test_package_failed_signature_preserves_existing_download(self):
        output = self.packaging_sources()
        output.mkdir()
        existing = output / "GUMMY-macOS.zip"
        existing.write_text("previous download")
        self.env["GUMMY_TEST_SIGN_EXIT"] = "1"
        result = self.run_script("package_mac.sh", "--build-dir", self.build, "--output-dir", output)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(existing.read_text(), "previous download")

    def test_package_archive_failure_preserves_existing_download(self):
        output = self.packaging_sources()
        output.mkdir()
        existing = output / "GUMMY-macOS.zip"
        existing.write_text("previous download")
        self.env["GUMMY_TEST_ARCHIVE_EXIT"] = "9"
        result = self.run_script("package_mac.sh", "--build-dir", self.build, "--output-dir", output)
        self.assertEqual(result.returncode, 9, result.stderr)
        self.assertEqual(existing.read_text(), "previous download")

    def test_packaged_launchers_install_validate_and_uninstall_without_build_tools(self):
        output = self.packaging_sources()
        result = self.run_script("package_mac.sh", "--build-dir", self.build, "--output-dir", output)
        self.assertEqual(result.returncode, 0, result.stderr)
        extracted = self.root / "extracted folder with spaces"
        with zipfile.ZipFile(output / "GUMMY-macOS.zip") as z:
            z.extractall(extracted)
            for entry in z.infolist():
                (extracted / entry.filename).chmod(entry.external_attr >> 16)
        package = extracted / "GUMMY-macOS"
        for action, input_text in (("Install", "\n"), ("Validate", "\n"), ("Uninstall", "REMOVE\n\n")):
            with self.subTest(action=action):
                result = subprocess.run(["bash", str(package / (action + " GUMMY.command"))],
                                        env=self.env, cwd=self.root, input=input_text, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                if action == "Install": self.assertTrue(all(p.exists() for p in self.installed_paths()))
        self.assertFalse(any(p.exists() for p in self.installed_paths()))
        self.assertEqual(self.commands("cmake"), [])
        self.assertEqual(self.commands("ctest"), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
