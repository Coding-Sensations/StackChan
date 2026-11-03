"""Real companion adapters + event/room models, synthetic ports only; no socket IO."""
from pathlib import Path
import sys,socket,unittest
from unittest.mock import patch
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'companion/host/streaming'))
from pydantic import StrictInt  # preload before asyncio debug timing, same SDK strict schema dependency
names=['test_hermes_adapter','test_https_wire','test_body_tools','test_deadline','test_pipeline','test_companion_events','test_provider_room_bridge','test_device_message_producer']
attempts=[]
def deny_socket(*a,**kw):
 attempts.append(True);raise AssertionError('real socket operation forbidden in source qualification')
with patch.object(socket,'create_connection',deny_socket),patch.object(socket.socket,'connect',deny_socket),patch.object(socket.socket,'connect_ex',deny_socket),patch.object(socket.socket,'bind',deny_socket),patch.object(socket.socket,'listen',deny_socket),patch.object(socket.socket,'sendto',deny_socket):
 suite=unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromName(n) for n in names)
 result=unittest.TextTestRunner(verbosity=2).run(suite)
assert result.testsRun>=113,'Every adopted route and outbound assertion must execute'
assert not attempts,'Caught exceptions must not hide real connection attempts'
assert not result.skipped,'No hidden skipped route assertions'
raise SystemExit(0 if result.wasSuccessful() else 1)
