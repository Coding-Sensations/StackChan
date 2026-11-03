"""Run all streaming fixtures from the tracked source closure; no module exclusions."""
from pathlib import Path
import sys
import os
import unittest

host = Path(__file__).resolve().parents[2] / 'companion/host'
root = Path(__file__).resolve().parents[2]
private = os.environ.get('HOME_LAB_ROOT')
if not private:
    raise SystemExit('HOME_LAB_ROOT must name the approved private source checkout; no exclusions allowed')
sys.path[:0] = [str(Path(private) / 'src'), str(root / 'bridge/host-adapter'),
               str(root / 'bridge/local-endpoint'), str(host / 'streaming'),
               str(host / 'native-audio'), str(host)]
suite = unittest.defaultTestLoader.discover(str(host / 'streaming'), pattern='test_*.py')
result = unittest.TextTestRunner(verbosity=2).run(suite)
if result.skipped:
    raise SystemExit(f'Required Python fixtures skipped: {result.skipped}')
raise SystemExit(0 if result.wasSuccessful() else 1)
