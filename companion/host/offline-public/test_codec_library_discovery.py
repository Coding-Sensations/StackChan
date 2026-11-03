"""Exercise codec construction with simulated loaders; no native library is loaded."""
import ctypes
import importlib.util
import os
from pathlib import Path
import unittest
from unittest.mock import Mock, patch


SOURCE = Path(__file__).resolve().parents[3] / 'bridge/local-endpoint/codec.py'
spec = importlib.util.spec_from_file_location('codec_discovery_fixture', SOURCE)
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)


def library():
    result = Mock()
    result.opus_encoder_create.return_value = 1
    result.opus_decoder_create.return_value = 2
    def allocate(context, *args):
        ctypes.cast(context, ctypes.POINTER(ctypes.c_void_p))[0] = 3
        return 0
    result.swr_alloc_set_opts2.side_effect = allocate
    result.swr_init.return_value = 0
    return result


class CodecLibraryDiscovery(unittest.TestCase):
    def construct(self, platform, available, discovered, environment=None):
        libraries = {path: library() for path in available}
        def load(path):
            if path not in libraries:
                raise OSError('library unavailable: ' + str(path))
            return libraries[path]
        with patch.dict(os.environ, environment or {}, clear=True), \
             patch.object(codec.sys, 'platform', platform), \
             patch('ctypes.util.find_library', side_effect=discovered.get), \
             patch.object(codec.C, 'CDLL', side_effect=load):
            opus = codec.OpusCodec(16000)
            resampler = codec.Resampler(16000, 48000)
            opus.close()
            resampler.close()
            return opus.lib, resampler.lib, resampler.av, libraries

    def test_intel_macos_uses_discovered_libraries_outside_apple_silicon_prefix(self):
        names = {'opus': '/usr/local/opt/opus/lib/libopus.0.dylib',
                 'swresample': '/usr/local/opt/ffmpeg/lib/libswresample.6.dylib',
                 'avutil': '/usr/local/opt/ffmpeg/lib/libavutil.60.dylib'}
        opus, resample, avutil, libraries = self.construct('darwin', names.values(), names)
        self.assertIs(opus, libraries[names['opus']])
        self.assertIs(resample, libraries[names['swresample']])
        self.assertIs(avutil, libraries[names['avutil']])

    def test_macos_keeps_both_homebrew_prefixes_when_discovery_misses(self):
        for prefix in ('/opt/homebrew/lib', '/usr/local/lib'):
            with self.subTest(prefix=prefix):
                paths = [prefix + '/libopus.dylib', prefix + '/libswresample.dylib', prefix + '/libavutil.dylib']
                opus, resample, avutil, libraries = self.construct('darwin', paths, {})
                self.assertEqual((opus, resample, avutil), tuple(libraries[path] for path in paths))

    def test_linux_uses_versioned_sonames_from_discovery(self):
        names = {'opus': 'libopus.so.0', 'swresample': 'libswresample.so.4', 'avutil': 'libavutil.so.58'}
        opus, resample, avutil, libraries = self.construct('linux', names.values(), names)
        self.assertEqual((opus, resample, avutil), tuple(libraries[names[name]] for name in ('opus', 'swresample', 'avutil')))

    def test_linux_can_use_loader_search_when_discovery_misses(self):
        paths = ['libopus.so', 'libswresample.so', 'libavutil.so']
        opus, resample, avutil, libraries = self.construct('linux', paths, {})
        self.assertEqual((opus, resample, avutil), tuple(libraries[path] for path in paths))

    def test_explicit_custom_paths_with_spaces_take_precedence(self):
        names = {'opus': '/custom codec/lib opus.dylib', 'swresample': '/custom codec/lib swresample.dylib',
                 'avutil': '/custom codec/lib avutil.dylib'}
        environment = {'STACKCHAN_' + name.upper() + '_LIBRARY': path for name, path in names.items()}
        opus, resample, avutil, libraries = self.construct('darwin', names.values(), {}, environment)
        self.assertEqual((opus, resample, avutil), tuple(libraries[names[name]] for name in ('opus', 'swresample', 'avutil')))

    def test_invalid_explicit_paths_fail_without_silent_fallback(self):
        for value in ('', 'relative/libopus.dylib', '/missing/libopus.dylib'):
            with self.subTest(value=value), patch.dict(os.environ, {'STACKCHAN_OPUS_LIBRARY': value}, clear=True), \
                 patch.object(codec.C, 'CDLL', side_effect=OSError('not found')) as load:
                with self.assertRaisesRegex(OSError, 'STACKCHAN_OPUS_LIBRARY'):
                    codec.OpusCodec(16000)
                self.assertEqual(load.call_count, int(value.startswith('/')))

    def test_loader_failure_reports_the_library_and_configuration_option(self):
        with patch.dict(os.environ, {}, clear=True), patch.object(codec.sys, 'platform', 'darwin'), \
             patch('ctypes.util.find_library', return_value=None), \
             patch.object(codec.C, 'CDLL', side_effect=OSError('wrong architecture')):
            with self.assertRaisesRegex(OSError, 'opus.*STACKCHAN_OPUS_LIBRARY'):
                codec.OpusCodec(16000)

    def test_unloadable_discovered_library_falls_back_to_matching_macos_library(self):
        names = {'opus': '/wrong-architecture/libopus.dylib', 'swresample': '/wrong-architecture/libswresample.dylib',
                 'avutil': '/wrong-architecture/libavutil.dylib'}
        paths = ['/usr/local/lib/libopus.dylib', '/usr/local/lib/libswresample.dylib', '/usr/local/lib/libavutil.dylib']
        opus, resample, avutil, libraries = self.construct('darwin', paths, names)
        self.assertEqual((opus, resample, avutil), tuple(libraries[path] for path in paths))


if __name__ == '__main__':
    unittest.main()
