#!/usr/bin/env python3
"""Local deterministic stock-restoration checks; no services or device access."""
from pathlib import Path
import argparse, json, subprocess, tempfile
repo = Path(__file__).resolve().parents[3]
tests = Path(__file__).resolve().parent
main = repo / "firmware/main"
parser = argparse.ArgumentParser()
parser.add_argument("--compiler", default="clang++")
parser.add_argument("--receipt", type=Path)
args = parser.parse_args()
checks = []
subprocess.run(["python3", str(tests / "verify_stock_menu_hierarchy.py")], check=True)
checks.append("menu_hierarchy")
subprocess.run(["python3", str(tests / "test_pr1_review_regressions.py")], check=True)
checks.append("pr1_review_regressions")
with tempfile.TemporaryDirectory(prefix="stackchan-stock-tests-") as temporary:
    temp = Path(temporary)
    generated = temp / "backend"
    subprocess.run(["python3", str(main / "apps/stock_agent/generate_backend.py"), "--xiaozhi", str(repo / "firmware/xiaozhi-esp32/main"), "--wifi", str(repo / "firmware/managed_components/78__esp-wifi-connect"), "--output", str(generated)], check=True)
    mooncake=repo / "firmware/components/mooncake/src"
    cases = [
        ("shell_teardown", [tests / "test_shell_teardown.cpp"] + sorted(mooncake.rglob("*.cpp")), [main, mooncake]),
        ("usb_owner", [tests / "test_usb_owner.cpp"], [main]),
        ("menu_navigation", [tests / "test_menu_navigation.cpp", main / "apps/stock_shell/navigation.cc"], [tests / "menu-fixture", main]),
        ("advanced_config", [repo / "firmware/tests/advanced_config_test.cpp"], [main]),
        ("firmware_policy", [repo / "firmware/tests/firmware_install_policy_test.cpp"], [main]),
        ("credentials", [tests / "test_wifi_credentials.cpp"], [main]),
        ("wifi_store", [tests / "test_wifi_store.cpp", generated / "stock_ssid_manager.cc"], [tests / "wifi-store-fixture", main, repo / "firmware/managed_components/78__esp-wifi-connect/include"]),
        ("firmware_migration", [tests / "test_firmware_migration.cpp", generated / "stock_ssid_manager.cc", main / "apps/stock_services/firmware_install_migration.cc"], [tests / "wifi-store-fixture", main, repo / "firmware/managed_components/78__esp-wifi-connect/include"]),
        ("scoped_settings", [tests / "test_readonly.cpp", main / "companion/readonly_settings.cc"], [tests, main, repo / "firmware/xiaozhi-esp32/main"]),
        ("tools_ui", [tests / "test_tools_ui.cpp"], [main / "apps/stock_shell", main / "companion"]),
        ("stock_models", [tests / "test_stock_models.cpp"], [main / "apps/stock_shell"]),
    ]
    for name, sources, includes in cases:
        binary = temp / name
        command = [args.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror"]
        # Match ESP-IDF's existing vendor signed-index comparison exemption.
        if name in ("wifi_store", "firmware_migration", "shell_teardown"): command.append("-Wno-sign-compare")
        if name in ("shell_teardown", "usb_owner"): command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        command += ["-I" + str(path) for path in includes]
        command += [str(path) for path in sources] + ["-o", str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
        checks.append(name)
receipt = {"result":"passed", "checks":checks, "source_revision":subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip(), "live_network":False, "device_access":False}
if args.receipt:
    args.receipt.write_text(json.dumps(receipt, indent=2) + "\n")
print(json.dumps(receipt, indent=2))
