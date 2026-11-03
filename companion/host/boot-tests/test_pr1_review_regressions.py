#!/usr/bin/env python3
"""Exercise the reviewed catalog and menu checks without device or service IO."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
TESTS = Path(__file__).resolve().parent
CATALOG = REPO / "firmware/main/apps/app_launcher/view/effect_catalog"
UPSTREAM = "1b5765599fba8aaad1811d9a79358ccc7051f5f3"


def run(*command, cwd=None):
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True)


class ReviewRegressions(unittest.TestCase):
    def check_euler_pixels(self, catalog, temporary):
        # After reset, the first frame advances fluidTime to 0.05. Hand-derived
        # RGB565 at (0,0) is (12,0,139), and at (0,8) is (12,40,138).
        # Integer abs truncation removes both fractional red/green components.
        source = temporary / "euler_pixels.cpp"
        source.write_text(r'''
#include "effect_catalog.hpp"
#include "raster.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
// ESP32-S3 libstdc++ exposes the integer C abs fallback here. macOS libc++
// additionally exposes floating overloads globally. Model the verified target
// fallback at the standard-library boundary while executing the real renderer.
namespace effect_catalog { namespace {
int abs(int value) { return std::abs(value); }
}}
#include "effect_catalog.cpp"
int main() {
    uint16_t effect = 0;
    while (effect < effect_catalog::Count() &&
           std::strcmp(effect_catalog::Name(effect), "Euler2d")) ++effect;
    if (effect == effect_catalog::Count()) return 2;
    std::vector<uint16_t> pixels(320 * 240);
    effect_catalog::Reset(effect);
    if (!effect_catalog::Render(effect, 0, pixels.data())) return 3;
    if (pixels[0] != 0x0811 || pixels[8 * 320] != 0x0951) {
        std::fprintf(stderr, "fractional Euler velocity colors: %04x %04x\n",
                     pixels[0], pixels[8 * 320]);
        return 1;
    }
}
''')
        binary = temporary / "euler_pixels"
        compiled = run("clang++", "-std=c++17", "-O1", "-Wno-absolute-value",
                       "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                       "-I" + str(catalog), str(source), "-o", str(binary))
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        rendered = run(str(binary))
        self.assertEqual(rendered.returncode, 0, rendered.stderr)

    def test_checked_in_catalog_preserves_fractional_velocity_colors(self):
        with tempfile.TemporaryDirectory(prefix="stackchan-pr1-pixels-") as tmp:
            self.check_euler_pixels(CATALOG, Path(tmp))

    def test_regenerated_catalog_preserves_fractional_velocity_colors(self):
        with tempfile.TemporaryDirectory(prefix="stackchan-pr1-generator-") as tmp:
            tmp = Path(tmp)
            catalog = tmp / "effect_catalog"
            catalog.mkdir()
            for name in ("generate_catalog.py", "upstream-source.txt", "raster.hpp"):
                shutil.copyfile(CATALOG / name, catalog / name)
            shutil.copyfile(CATALOG.parent / "screensaver_filter.hpp",
                            tmp / "screensaver_filter.hpp")
            generated = run(sys.executable, str(catalog / "generate_catalog.py"))
            self.assertEqual(generated.returncode, 0, generated.stderr)
            self.check_euler_pixels(catalog, tmp)

    def test_menu_check_accepts_the_actual_screensaver_row(self):
        checked = run(sys.executable, str(TESTS / "verify_stock_menu_hierarchy.py"))
        self.assertEqual(checked.returncode, 0, checked.stderr)

    def test_menu_check_still_rejects_unrelated_rows_and_navigation_reversions(self):
        # Give the actual checker controlled source inputs while retaining real
        # upstream Git blobs for its compatibility comparisons. Only /tmp changes.
        with tempfile.TemporaryDirectory(prefix="stackchan-pr1-menu-") as tmp:
            fixture = Path(tmp) / "repo"
            cloned = run("git", "clone", "--shared", "--no-checkout", "--quiet",
                         str(REPO), str(fixture))
            self.assertEqual(cloned.returncode, 0, cloned.stderr)
            directories = ["firmware/main/apps/app_setup/view",
                           "firmware/main/apps/app_launcher/view",
                           "firmware/main/apps/common"]
            paths = subprocess.check_output(
                ["git", "-C", str(REPO), "ls-tree", "-r", "--name-only",
                 UPSTREAM, "--", *directories], text=True).splitlines()
            setup = "firmware/main/apps/app_setup/app_setup.cpp"
            paths += [setup, "firmware/main/apps/stock_shell/stock_apps.cc",
                      "companion/host/boot-tests/verify_stock_menu_hierarchy.py"]
            for name in paths:
                target = fixture / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(REPO / name, target)
            original = (REPO / setup).read_text()
            for label, replacement in (("Brightness", "Unexpected Brightness"),
                                       ("Brightness", "Screensaver"),
                                       ("Testing service", "Unexpected service"),
                                       ("Stackchan", "AI.Agent")):
                with self.subTest(label=label, replacement=replacement):
                    (fixture / setup).write_text(original.replace(
                        '"' + label + '"', '"' + replacement + '"'))
                    checked = run(sys.executable, str(
                        fixture / "companion/host/boot-tests/verify_stock_menu_hierarchy.py"))
                    self.assertNotEqual(checked.returncode, 0)
            (fixture / setup).write_text(original)
            view = "firmware/main/apps/app_launcher/view/screensaver.cpp"
            for revision in ("4a24491761ffab19f3df758c93ebbdada4c29ddc",
                             "25c65868841bdf8d640dffac66543265fc2e9137"):
                with self.subTest(reverted_screensaver=revision):
                    (fixture / view).write_bytes(subprocess.check_output(
                        ["git", "-C", str(REPO), "show", revision + ":" + view]))
                    checked = run(sys.executable, str(
                        fixture / "companion/host/boot-tests/verify_stock_menu_hierarchy.py"))
                    self.assertNotEqual(checked.returncode, 0)


if __name__ == "__main__":
    unittest.main()
