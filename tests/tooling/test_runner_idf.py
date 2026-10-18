"""Regression checks for relocated ESP-IDF demo workspaces and tool exports."""

import subprocess
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import run


class DemoWorkspaceTests(unittest.TestCase):
    def check_cache(self, *, stale_application=False, stale_bootloader=False):
        build = run.BUILD_ROOT / "test-idf-workspace"
        source = run.PROJECT_ROOT / "fw-esp32" / "demo-uart"
        boot_source = run._idf_paths()[0] / "components" / "bootloader" / "subproject"
        caches = {}
        for directory, home, stale in (
            (build, source, stale_application),
            (build / "bootloader", boot_source, stale_bootloader),
        ):
            cached_directory = Path("C:/old-project") if stale else directory
            caches[directory / "CMakeCache.txt"] = (
                f"CMAKE_CACHEFILE_DIR:INTERNAL={cached_directory.as_posix()}\n"
                f"CMAKE_HOME_DIRECTORY:INTERNAL={home.as_posix()}\n"
            )
        with (
            patch.object(Path, "is_file", lambda path: path in caches),
            patch.object(Path, "read_text", lambda path, **kwargs: caches[path]),
            patch.object(Path, "rename") as rename,
        ):
            run._archive_relocated_idf_build(build, source)
        return rename

    def test_fresh_caches_are_kept(self):
        self.check_cache().assert_not_called()

    def test_relocated_application_is_archived(self):
        self.check_cache(stale_application=True).assert_called_once()

    def test_stale_bootloader_is_found_after_application_reconfigure(self):
        self.check_cache(stale_bootloader=True).assert_called_once()


class ExportTests(unittest.TestCase):
    def test_export_failure_contains_diagnostics_and_setup_command(self):
        result = subprocess.CompletedProcess([], 1, "", "missing required tool")
        with (
            patch.object(Path, "is_file", return_value=True),
            patch.object(run.subprocess, "run", return_value=result),
        ):
            with self.assertRaisesRegex(RuntimeError, "setup-esp32") as error:
                run._import_idf_environment({})
        self.assertIn("missing required tool", str(error.exception))

    def test_export_uses_idf_python_and_expands_windows_path(self):
        result = subprocess.CompletedProcess([], 0, "PATH=C:\\idf-bin;%PATH%\n", "")
        environment = {"Path": "C:\\original"}
        python = Path("C:/idf-python/python.exe")
        with (
            patch.object(Path, "is_file", return_value=True),
            patch.object(run, "_idf_python", return_value=python),
            patch.object(run.subprocess, "run", return_value=result) as execute,
        ):
            run._import_idf_environment(environment)
        self.assertEqual(execute.call_args.args[0][0], str(python))
        self.assertEqual(environment["Path"], "C:\\idf-bin;C:\\original")


class PortTests(unittest.TestCase):
    def test_single_native_port_is_selected(self):
        result = subprocess.CompletedProcess([], 0, '["COM10"]', "")
        environment = {}
        with patch.object(run.subprocess, "run", return_value=result):
            run._prefer_esp32_native_port(environment)
        self.assertEqual(environment["ESPPORT"], "COM10")

    def test_multiple_native_ports_are_not_arbitrarily_selected(self):
        result = subprocess.CompletedProcess([], 0, '["COM10", "COM11"]', "")
        environment = {}
        with patch.object(run.subprocess, "run", return_value=result):
            run._prefer_esp32_native_port(environment)
        self.assertNotIn("ESPPORT", environment)

    def test_explicit_environment_port_is_preserved(self):
        environment = {"ESPPORT": "COM11"}
        with patch.object(run.subprocess, "run") as execute:
            run._prefer_esp32_native_port(environment)
        execute.assert_not_called()
        self.assertEqual(environment["ESPPORT"], "COM11")


if __name__ == "__main__":
    unittest.main()
