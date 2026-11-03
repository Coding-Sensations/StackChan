"""Offline source-selection tests; no SDK, device or runtime activation."""
from pathlib import Path
import importlib.util
import json
import tempfile
import unittest

FIRMWARE = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('profile_tool', FIRMWARE/'tools/build_profile.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class Profiles(unittest.TestCase):
    def test_default_full_keeps_existing_optional_groups(self):
        result = tool.resolve('full')
        self.assertEqual(set(result['enabled']), set(tool.FEATURES))
        self.assertEqual(result['excluded_sources'], ['build_profiles/no_music_callbacks.cc'])

    def test_no_games_parks_only_games_and_keeps_other_features(self):
        result = tool.resolve('no-games')
        self.assertEqual(set(result['enabled']), set(tool.FEATURES)-{'games'})
        self.assertEqual(result['mandatory'], ['stock','mira','settings','recovery'])
        self.assertEqual(result['excluded_sources'], ['apps/pet/pet_app.cc','build_profiles/no_music_callbacks.cc'])
        self.assertEqual(result['definitions']['STACKCHAN_BUILD_GAMES'], 0)
        self.assertEqual(result['definitions']['STACKCHAN_BUILD_MUSIC'], 1)
        with self.assertRaises(ValueError):
            tool.resolve('no-games', {'schema':1,'features':[]})

    def test_core_retains_mandatory_and_excludes_real_music_sources(self):
        result = tool.resolve('mira-core')
        self.assertEqual(result['mandatory'], ['stock', 'mira', 'settings', 'recovery'])
        self.assertEqual(result['enabled'], [])
        self.assertIn('apps/local_tools/local_audio.cc', result['excluded_sources'])
        self.assertIn('apps/instruments/instrument_apps.cc', result['excluded_sources'])
        self.assertIn('apps/stock_shell/provider_companion_app.cc', result['excluded_sources'])
        self.assertNotIn('companion/boot.cc', result['excluded_sources'])

    def test_custom_validates_selection_and_keeps_dependency_closure(self):
        result = tool.resolve('custom', {'schema': 1, 'features': ['music', 'muse']})
        self.assertEqual(result['enabled'], ['muse', 'music'])
        self.assertNotIn('apps/local_tools/local_audio.cc', result['excluded_sources'])
        self.assertNotIn('usb_composite_v2/product_controls.cc', result['excluded_sources'])
        self.assertNotIn('apps/stock_shell/provider_companion_app.cc', result['excluded_sources'])

    def test_bad_or_ambiguous_selection_fails_closed(self):
        for profile, config in [('typo', None), ('custom', None),
                ('full', {'schema': 1, 'features': []}),
                ('custom', {'schema': 2, 'features': []}),
                ('custom', {'schema': True, 'features': []}),
                ('custom', {'schema': 1, 'features': ['stock']}),
                ('custom', {'schema': 1, 'features': ['music', 'music']}),
                ('custom', {'schema': 1, 'features': ['${evil}']}),
                ('custom', {'schema': 1, 'features': [], 'permissions': True}),
                ('custom', {'schema': 1, 'features': 'music'})]:
            with self.subTest(profile=profile, config=config), self.assertRaises(ValueError):
                tool.resolve(profile, config)

    def test_duplicate_json_key_rejected(self):
        with self.assertRaises(ValueError):
            tool.parse_selection('{"schema":1,"features":[],"features":["music"]}')

    def test_direct_muse_cannot_be_partially_excluded(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); p = root/'companion/muse_direct_device.cc'
            p.parent.mkdir(); p.write_text('// fixture')
            with self.assertRaisesRegex(ValueError, 'Muse direct'):
                tool.resolve('mira-core', main_root=root)
            self.assertIn('muse', tool.resolve('full', main_root=root)['enabled'])

    def test_receipt_and_cmake_are_deterministic_and_no_unauthorized_state(self):
        a = tool.resolve('custom', {'schema':1, 'features':['games','music']})
        b = tool.resolve('custom', {'schema':1, 'features':['music','games']})
        self.assertEqual(a, b)
        cmake = tool.render_cmake(a)
        self.assertIn('STACKCHAN_BUILD_MUSIC=1', cmake)
        self.assertIn('STACKCHAN_BUILD_MUSE=0', cmake)
        self.assertNotIn('NVS', cmake)
        self.assertNotIn('execute_process', cmake)


if __name__ == '__main__':
    unittest.main()
