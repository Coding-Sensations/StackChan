"""Run production Dart logic with inert Flutter logging/annotations and RSA boundary.

Requires an existing Dart executable. Does not install dependencies, fetch Pub
packages, bind sockets, use credentials or run the Flutter application.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--dart', default='dart')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='stackchan-app-transport-') as temporary:
    directory = Path(temporary)
    package = directory / 'stack_chan'
    for name in ['network/local_area_network_service.dart', 'network/device_message_transport.dart',
                 'model/msg_type.dart', 'util/mac_address_validator.dart']:
        target = package / 'lib' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / 'app/lib' / name, target)
    (package / 'lib/util/rsa_util.dart').write_text('''class RsaUtil {
  static String decryptStackChanBlue(String value) => throw StateError('Unexpected crypto IO');
}
''')
    flutter = directory / 'flutter/lib'
    flutter.mkdir(parents=True)
    (flutter / 'foundation.dart').write_text('''class _VisibleForTesting { const _VisibleForTesting(); }
const visibleForTesting = _VisibleForTesting();
void debugPrint(String? value, {int? wrapWidth}) {}
''')
    config = directory / '.dart_tool/package_config.json'
    config.parent.mkdir()
    config.write_text(json.dumps({'configVersion': 2, 'packages': [
        {'name': name, 'rootUri': (directory / name).as_uri() + '/',
         'packageUri': 'lib/', 'languageVersion': '3.13'} for name in ['stack_chan', 'flutter']]}))
    fixture = directory / 'checks.dart'
    shutil.copyfile(root / 'app/test/offline_transport_checks.dart', fixture)
    subprocess.run([args.dart, '--enable-asserts', str(fixture)],
                   cwd=directory, check=True, timeout=60)
