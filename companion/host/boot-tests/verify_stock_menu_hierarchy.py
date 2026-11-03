#!/usr/bin/env python3
"""Assert official menu structure and one nested Tools app, without device I/O."""
from pathlib import Path
import subprocess,re,json,hashlib
repo=Path(__file__).resolve().parents[3]
base="1b5765599fba8aaad1811d9a79358ccc7051f5f3"
# Preserve main's later Network Test row as well as the lifecycle additions.
main_menu_base="eff0d6b5109663faec8f1d98848b4b0e5246bbfc"
screensaver_base="4a24491761ffab19f3df758c93ebbdada4c29ddc"
screensaver_views={"firmware/main/apps/app_launcher/view/view.h"}
# Reviewed navigation view/lifecycle snapshots. Inline hashes keep this check
# self-contained without requiring the unpublished source candidate's Git object.
navigation_views={
 # Bottom-swipe Home gains initially-hidden companion mode and transient status;
 # other callers retain default first-show behavior (companion_home_test.py).
 'firmware/main/apps/common/home_indicator/home_indicator.cpp':'489ad3496c004df47081277156c3fb3c567443bb2787b2fc6b358eb638523731',
 'firmware/main/apps/common/home_indicator/home_indicator.h':'ca2949cec90f6ea25b46dda5231840701596b5c4e4fdc0b8626fb91a2e8ee752',
 "firmware/main/apps/app_launcher/view/screensaver.cpp":"52900c120f8e1d1460b29e0f830cf2c5b1b3df2bbe526dfd2420dfd8449097af",
 "firmware/main/apps/common/toast/toast.cpp":"27aba8cce159c046fcf31374990a1598f9fc0a489a368d76b93481504ced03ca",
 "firmware/main/apps/common/toast/toast.h":"d6ea6796042b15bc14e0a468ba1c7df93280c3fd820fc1a60b58385f940709e2",
}
def original(name,revision=base):return subprocess.check_output(["git","-C",str(repo),"show",revision+":"+name])
def menu_labels(source):
 source=source[source.index("    _menu_sections = {"):]
 source=source[:source.index("\n    };")]
 source="\n".join(line.split("//",1)[0] for line in source.splitlines())
 return re.findall(r'"([^"\n]*)"',source)
setup="firmware/main/apps/app_setup/app_setup.cpp"
expected_setup=menu_labels(original(setup,main_menu_base).decode())
# Keep official labels/order, the new Device row, and the requested agent rename.
expected_setup.insert(expected_setup.index("Brightness"), "Screensaver")
expected_setup[expected_setup.index("AI.Agent")]="Stackchan"
assert menu_labels((repo/setup).read_text())==expected_setup
for directory in ["firmware/main/apps/app_setup/view","firmware/main/apps/app_launcher/view","firmware/main/apps/common"]:
 for name in subprocess.check_output(["git","-C",str(repo),"ls-tree","-r","--name-only",base,"--",directory],text=True).splitlines():
  data=(repo/name).read_bytes()
  if name in navigation_views:
   assert hashlib.sha256(data).hexdigest()==navigation_views[name],name
  else:
   # Preserve the integration header and all other original view comparisons.
   assert data==original(name,screensaver_base if name in screensaver_views else base),name
shell=(repo/"firmware/main/apps/stock_shell/stock_apps.cc").read_text()
registration=shell[shell.index("[[noreturn]] void Run()") :]
assert registration.count("std::make_unique<ToolsApp>()")==1
assert "network_setup::MakeApp()" not in registration and "stock_app_center::MakeApp" not in registration
assert "for(auto feature:LauncherNativeFeatures)" in registration
assert "IsSetupFeature(*feature)" in registration
assert "native_ids.at(" not in registration
assert registration.count("stock_tools::MakeJointCompanionApp()")==1
order=[registration.index(text) for text in ["stock_agent::MakeApp()","for(auto feature:LauncherNativeFeatures)","std::make_unique<AppSetup>()","stock_tools::MakeJointCompanionApp()","std::make_unique<ToolsApp>()","std::make_unique<GamesApp>()"]]
assert order==sorted(order)
assert "Welcome Setup" not in (repo/setup).read_text()
# Dedicated tool pages are members of the one Tools app, not launcher apps.
tools=shell[shell.index("class ToolsApp"):shell.index("class GamesApp")]
for page in ["Home","Infrared","Learn","Send","Nfc","Edit","Emulate"]:assert "case Screen::"+page+":" in tools
for action in ["OpenInfrared","OpenNfc","OpenLearn","OpenSend","OpenEdit"]:assert action in tools
assert "installApp" not in tools
print(json.dumps({"result":"passed","setup":"official menu + Screensaver, AI.Agent renamed Stackchan","launcher":"official registration prefix; Companions visual route checked separately","tools":"one Tools tile -> selection -> dedicated graphical pages","device_access":False}))
