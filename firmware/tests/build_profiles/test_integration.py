"""Exercise real CMake filter and guarded production factory/catalog code offline."""
from pathlib import Path
import importlib.util
import json
import shutil
import subprocess
import tempfile
import unittest

FIRMWARE=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('profiles',FIRMWARE/'tools/build_profile.py')
profiles=importlib.util.module_from_spec(spec);spec.loader.exec_module(profiles)


class Integration(unittest.TestCase):
    def test_early_scan_loads_parent_selection_in_fresh_process(self):
        resolver=FIRMWARE/'build_profiles.cmake'
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for profile in ['mira-core','custom','no-games']:
                build=root/profile;build.mkdir()
                selection=root/'selected.json';selection.write_text('{"schema":1,"features":["music"]}')
                parent=root/'parent.cmake'
                parent.write_text('cmake_minimum_required(VERSION 3.16)\n'
                    +f'set(CMAKE_BINARY_DIR "{build}")\nset(STACKCHAN_BUILD_PROFILE {profile})\n'
                    +(f'set(STACKCHAN_FEATURE_MANIFEST "{selection}")\n' if profile=='custom' else '')
                    +f'include("{resolver}")\n')
                subprocess.run([shutil.which('cmake'),'-P',str(parent)],check=True,capture_output=True)
                receipt=build/'stackchan-profile/feature-selection.json';before=receipt.read_bytes()
                child=root/'child.cmake'
                child.write_text('cmake_minimum_required(VERSION 3.16)\nset(CMAKE_BUILD_EARLY_EXPANSION 1)\n'
                    +f'function(idf_build_get_property out key)\n if(key STREQUAL "BUILD_DIR")\n set(${{out}} "{build}" PARENT_SCOPE)\n endif()\nendfunction()\n'
                    +f'include("{resolver}")\n'
                    +f'if(NOT STACKCHAN_PROFILE_GAMES STREQUAL "0")\nmessage(FATAL_ERROR "parent profile lost")\nendif()\n')
                subprocess.run([shutil.which('cmake'),'-P',str(child)],check=True,capture_output=True,cwd=root)
                self.assertEqual(receipt.read_bytes(),before)
                # Missing parent receipt must fail, never regenerate default full.
                receipt.with_suffix('.cmake').unlink()
                result=subprocess.run([shutil.which('cmake'),'-P',str(child)],capture_output=True)
                self.assertNotEqual(result.returncode,0)

    def test_real_registration_links_without_parked_factories(self):
        source=(FIRMWARE/'main/apps/stock_shell/stock_apps.cc').read_text()
        registration=source[source.index('    // Official 1.5.1 launcher order;'):source.index('    bool stopped_preview=false;')]
        for keys in [list(profiles.FEATURES),sorted(set(profiles.FEATURES)-{'games'}),[],['music'],['games'],['muse']]:
            selected=profiles.resolve('custom',{'schema':1,'features':keys})
            declarations=r'''
#include <apps/stock_shell/provider_catalog.hpp>
#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <string>
struct App {std::string name;explicit App(std::string n):name(n){}virtual ~App()=default;};
struct Shell {std::set<std::string> names;int installApp(std::unique_ptr<App> a){assert(a);names.insert(a->name);return int(names.size());}};
struct AppSetup:App{AppSetup():App("Settings"){};};
#if STACKCHAN_BUILD_TOOLBOX
struct ToolsApp:App{ToolsApp():App("TOOLS"){};};
#endif
#if STACKCHAN_BUILD_GAMES
struct GamesApp:App{GamesApp():App("GAMES"){};};
#endif
namespace stock_agent{auto MakeApp(){return std::make_unique<App>("Stackchan");}}
namespace stock_services{enum class Feature{Avatar};auto MakeApp(Feature){return std::make_unique<App>("AVATAR");}}
const stock_services::Feature LauncherNativeFeatures[]={stock_services::Feature::Avatar};
namespace stock_tools {
auto MakeJointCompanionApp(){return std::make_unique<App>("Mira");}
auto MakeSdStatusApp(){return std::make_unique<App>("SD STATUS");}
#if STACKCHAN_BUILD_PROVIDERS
auto MakeProviderCompanionApp(companion_modes::Provider p){return std::make_unique<App>(companion_modes::Find(p)->label);}
#endif
#if STACKCHAN_BUILD_LED_CONTROLS
auto MakeLedExpressionApp(){return std::make_unique<App>("LED EXPRESSIONS");}
#endif
#if STACKCHAN_BUILD_NETWORK_TOOLS
auto MakeProximityExpressionApp(){return std::make_unique<App>("PROXIMITY");}
auto MakeNetworkToolsApp(){return std::make_unique<App>("NETWORK TOOLS");}
#endif
}
namespace local_tools_ui {
#if STACKCHAN_BUILD_GAMES
auto MakePetApp(){return std::make_unique<App>("PET");}
#endif
#if STACKCHAN_BUILD_MUSIC
auto MakeAudioToolsApp(){return std::make_unique<App>("AUDIO TOOLS");}
#endif
}
#if STACKCHAN_BUILD_MUSIC
namespace instrument_ui{enum class Kind{Synth,Groovebox,Sampler};auto MakeApp(Kind k){return std::make_unique<App>("instrument"+std::to_string(int(k)));}}
namespace usb_composite_v2{auto MakeControlsApp(){return std::make_unique<App>("USB CONTROLS");}}
#endif
int main(){Shell shell;
'''
            assertions='assert(shell.names.count("Mira"));assert(shell.names.count("Stackchan"));assert(shell.names.count("Settings"));assert(shell.names.count("SD STATUS"));'
            for key,label in [('music','AUDIO TOOLS'),('games','PET'),('games','GAMES'),('toolbox','TOOLS'),('network_tools','NETWORK TOOLS'),('led_controls','LED EXPRESSIONS'),('muse','Muse')]:
                assertions+=f'assert(shell.names.count("{label}")=={int(key in keys)});'
            with tempfile.TemporaryDirectory() as tmp:
                p=Path(tmp);(p/'factory.cc').write_text(declarations+registration+assertions+'}')
                defs=[f'-D{k}={v}' for k,v in selected['definitions'].items()]
                subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(FIRMWARE/'main'),*defs,str(p/'factory.cc'),'-o',str(p/'factory')],check=True)
                subprocess.run([str(p/'factory')],check=True)

    def test_music_without_games_legacy_alias_has_no_pet_link(self):
        source=(FIRMWARE/'main/apps/local_tools/local_tools_app.cc').read_text()
        start=source.index('std::unique_ptr<mooncake::AppAbility> MakeApp()')
        end=source.index('\n}',start)+2
        alias=source[start:end]
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'alias.cc').write_text('#include <memory>\nnamespace mooncake{struct AppAbility{};}\n'+alias+'\nint main(){return MakeApp()?1:0;}')
            subprocess.run(['clang++','-std=c++17','-DSTACKCHAN_BUILD_GAMES=0',str(p/'alias.cc'),'-o',str(p/'alias')],check=True)
            subprocess.run([str(p/'alias')],check=True)

    def test_cmake_removes_absolute_and_relative_sources_after_forced_appends(self):
        main=FIRMWARE/'main'
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            all_sources=sorted({p for sources in profiles.FEATURES.values() for p in sources})
            all_sources+=['companion/boot.cc','apps/stock_shell/stock_apps.cc',
                          'apps/stock_shell/provider_companion_app.cc','apps/app_setup/app_setup.cpp']
            for profile in ['full','no-games','mira-core']:
                selected=profiles.resolve(profile)
                script=root/(profile+'.cmake');output=root/(profile+'.sources')
                units=all_sources+[str(main/p) for p in all_sources]
                script.write_text('cmake_minimum_required(VERSION 3.16)\n'
                    +f'set(CMAKE_CURRENT_SOURCE_DIR "{main}")\n'
                    +f'set(CMAKE_BINARY_DIR "{root/profile}")\n'
                    +f'set(STACKCHAN_BUILD_PROFILE "{profile}")\n'
                    +'set(SOURCES\n'+''.join(f' "{p}"\n' for p in units)+')\n'
                    +f'include("{main}/build_profiles/select.cmake")\n'
                    +f'file(WRITE "{output}" "${{SOURCES}}")\n')
                subprocess.run([shutil.which('cmake'),'-P',str(script)],check=True,capture_output=True)
                kept=output.read_text().split(';')
                for unit in all_sources:
                    self.assertEqual(unit in kept,unit not in selected['excluded_sources'],(profile,unit))
                    self.assertEqual(str(main/unit) in kept,unit not in selected['excluded_sources'],(profile,unit))
                self.assertEqual(str(main/'build_profiles/no_music_callbacks.cc') in kept,profile=='mira-core')
                receipt=json.loads((root/profile/'stackchan-profile/feature-selection.json').read_text())
                self.assertEqual(receipt,selected)

    def test_actual_catalog_all_provider_combinations_preserve_mira_stock_ids(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);cpp=p/'catalog.cc';binary=p/'catalog'
            for mask in range(16):
                definitions=[f'-DSTACKCHAN_BUILD_{key.upper()}={int(bool(mask&(1<<i)))}' for i,key in enumerate(profiles.PROVIDERS)]
                cpp.write_text('#include <apps/stock_shell/provider_catalog.hpp>\n#include <cassert>\n'
                    +'int main(){using namespace companion_modes;'
                    +'static_assert(int(Provider::Codex)==5);assert(Find(Provider::StackChan));assert(Find(Provider::Mira));'
                    +f'assert(Catalog.size()=={2+mask.bit_count()});'
                    +''.join(f'assert(bool(Find(Provider::{key.title()}))=={int(bool(mask&(1<<i)))});' for i,key in enumerate(profiles.PROVIDERS))+'}')
                subprocess.run(['clang++','-std=c++17','-I'+str(FIRMWARE/'main'),*definitions,str(cpp),'-o',str(binary)],check=True)
                subprocess.run([str(binary)],check=True)


if __name__=='__main__':unittest.main()
