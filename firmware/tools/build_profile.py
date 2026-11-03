"""Resolve pre-flash code inclusion only. Never invokes a build, device or installer."""
from pathlib import Path
import argparse
import hashlib
import json

# Whole source closures, not merely launcher labels. Shared stock/Mira facilities
# are deliberately not optional. The music group owns all local-audio consumers.
MUSE_SOURCES = ['companion/muse_cold_output.cc', 'companion/muse_direct_local_ui.cc', 'companion/muse_direct_local_port.cc',
                'companion/muse_direct_api.cc', 'companion/muse_direct_ble.cc',
                'companion/muse_direct_device.cc', 'companion/muse_direct_live.cc',
                'companion/muse_direct_native_api.cc', 'companion/muse_direct_native_socket.cc',
                'companion/muse_direct_runtime.cc', 'companion/muse_direct_store.cc',
                'companion/muse_direct_credential_custody.cc', 'companion/muse_voice_task.cc', 'companion/muse_voice_core_access.cc',
                'companion/muse_room_transport.cc']
FEATURES = {
    'hermes': [], 'muse': MUSE_SOURCES, 'muse_voice_protocol': ['companion/muse_cold_output.cc', 'companion/muse_voice_task.cc', 'companion/muse_voice_core_access.cc'], 'bounded_motion': [], 'claude': [], 'codex': [],
    'music': ['apps/instruments/instrument_apps.cc',
              'apps/instruments/midi_host/host_client.cc',
              'apps/local_tools/local_tools_app.cc',
              'apps/local_tools/local_audio.cc'],
    'games': ['apps/pet/pet_app.cc'],
    'toolbox': ['apps/stock_shell/stock_tools_hw.cc'],
    'network_tools': ['apps/network_sensor/network_sensor_apps.cc',
                      'apps/network_sensor/proximity_expression_app.cc',
                      'apps/network_sensor/imu_expression_adapter.cc'],
    'led_controls': ['apps/led_expression/led_apps.cc'],
}
PROVIDERS = ('hermes', 'muse', 'claude', 'codex')


def parse_selection(raw):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate selection key')
            result[key] = value
        return result
    return json.loads(raw, object_pairs_hook=unique)


def resolve(profile, selection=None, main_root=None):
    if profile not in ('full', 'no-games', 'mira-core', 'custom'):
        raise ValueError('Unknown build profile; use full, no-games, mira-core or custom')
    if profile == 'custom':
        if (not isinstance(selection, dict) or set(selection) != {'schema', 'features'}
                or type(selection['schema']) is not int or selection['schema'] != 1
                or not isinstance(selection['features'], list)
                or any(not isinstance(x, str) or x not in FEATURES for x in selection['features'])
                or len(selection['features']) != len(set(selection['features']))):
            raise ValueError('Invalid feature selection; schema 1 and unique allowlisted features required')
        enabled = set(selection['features'])
    else:
        if selection is not None:
            raise ValueError('Selection file requires custom profile')
        enabled = set(FEATURES) if profile in ('full','no-games') else set()
        if profile == 'no-games':
            enabled.remove('games')
    if 'muse_voice_protocol' in enabled and 'muse' not in enabled:
        raise ValueError('muse_voice_protocol requires muse')
    # This direct Muse revision has extra component/entrypoint/link-root edges.
    # The source-owned receipt pins the composed guards, not runtime authority.
    if main_root and 'muse' not in enabled:
        main_root=Path(main_root)
        implementations=sorted(p.relative_to(main_root).as_posix() for p in main_root.glob('companion/muse_*.cc'))
        if implementations:
            receipt=main_root/'build_profiles/muse-boundary.json'
            required={'main/CMakeLists.txt', 'main/apps/stock_shell/provider_companion_app.cc',
                      'main/companion/provider_room_view.hpp', 'components/muse_direct_protocol/CMakeLists.txt',
                      'main/feature_profiles/muse_disabled_boundary.cc'}
            try:
                boundary=parse_selection(receipt.read_text())
                valid=(isinstance(boundary,dict) and type(boundary.get('schema')) is int
                       and boundary['schema']==1 and isinstance(boundary.get('guard_hashes'),dict)
                       and set(boundary['guard_hashes'])==required
                       and all(hashlib.sha256((main_root.parent/path).read_bytes()).hexdigest()==sha
                               for path,sha in boundary['guard_hashes'].items())
                       and set(implementations).issubset(MUSE_SOURCES))
            except (ValueError, OSError, TypeError, AttributeError):
                valid=False
            if not valid:
                raise ValueError('Muse direct source detected without the matching reviewed build-profile boundary')
    excluded = sorted({path for key, sources in FEATURES.items() if key not in enabled for path in sources})
    if not enabled.intersection(PROVIDERS):
        excluded.append('apps/stock_shell/provider_companion_app.cc')
    if not enabled.intersection({'music', 'toolbox'}):
        excluded.append('usb_composite_v2/product_controls.cc')
    if 'music' in enabled:
        excluded.append('build_profiles/no_music_callbacks.cc')
    return {'schema': 1, 'profile': profile,
            'mandatory': ['stock', 'mira', 'settings', 'recovery'],
            'enabled': sorted(enabled), 'excluded_sources': sorted(excluded),
            'definitions': {f'STACKCHAN_BUILD_{key.upper()}': int(key in enabled) for key in FEATURES}}


def render_cmake(result):
    definitions = [f'{key}={value}' for key, value in result['definitions'].items()]
    # Every emitted token is a source-owned constant or a validated enum; no
    # user-supplied text is inserted into executable CMake.
    return ('# Generated pre-flash feature closure. No runtime policy.\n'
            + ''.join(f'set(STACKCHAN_PROFILE_{key.upper()} {int(key in result["enabled"])})\n' for key in FEATURES)
            + 'set(STACKCHAN_PROFILE_DEFINITIONS\n  ' + '\n  '.join(definitions) + '\n)\n'
            + 'set(STACKCHAN_PROFILE_EXCLUDED_SOURCES\n  '
            + '\n  '.join(result['excluded_sources']) + '\n)\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--selection', type=Path)
    parser.add_argument('--main-root', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        result = resolve(args.profile,
                         parse_selection(args.selection.read_text()) if args.selection else None,
                         args.main_root)
    except (ValueError, OSError) as error:
        parser.exit(2, f'Feature selection rejected: {error}\n')
    args.output.mkdir(parents=True, exist_ok=True)
    for name, data in [('feature-selection.json', json.dumps(result, indent=2, sort_keys=True)+'\n'),
                       ('feature-selection.cmake', render_cmake(result))]:
        destination = args.output/name
        if not destination.exists() or destination.read_text() != data:
            destination.write_text(data)
    print('Pre-flash profile: '+result['profile']+'; optional: '+(', '.join(result['enabled']) or 'none'))


if __name__ == '__main__':
    main()
