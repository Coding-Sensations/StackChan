// Production routing/listener checks with inert IO boundaries; no Flutter UI.
import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:stack_chan/model/msg_type.dart';
import 'package:stack_chan/network/device_message_transport.dart';
import 'package:stack_chan/network/local_area_network_service.dart';

void check(bool value, String message) {
  if (!value) throw StateError(message);
}

const mac = '001122334455';

class FakeLocal extends LocalAreaNetworkService {
  FakeLocal() : super.forTesting((value) => value);
  bool connected = false;
  final sent = <MsgType>[];

  @override
  bool sendConnectedMessage(MsgType type, {required String deviceMac, Uint8List? payload}) {
    check(deviceMac == mac, 'routing changed target');
    if (connected) sent.add(type);
    return connected;
  }
}

class FakeServer implements HttpServer {
  bool closed = false;
  @override
  int get port => 18443;
  @override
  StreamSubscription<HttpRequest> listen(void Function(HttpRequest)? onData,
      {Function? onError, void Function()? onDone, bool? cancelOnError}) =>
      const Stream<HttpRequest>.empty().listen(onData);
  @override
  Future<HttpServer> close({bool force = false}) async {
    closed = true;
    return this;
  }
  @override
  dynamic noSuchMethod(Invocation invocation) => throw StateError('Unexpected listener IO: ${invocation.memberName}');
}

Future<void> routing() async {
  final local = FakeLocal();
  bool camera = false, screen = false;
  int cameraStarts = 0, screenStarts = 0;
  final remote = <MsgType>[];
  final transport = DeviceMessageTransport(local: local, remote: (type, {data}) {
    check(String.fromCharCodes(data!) == mac, 'routing changed payload');
    remote.add(type);
    switch (type) {
      case MsgType.onCamera:
        if (!camera) cameraStarts++;
        camera = true;
        break;
      case MsgType.offCamera:
        camera = false;
        break;
      case MsgType.onPhoneScreen:
        if (!screen) screenStarts++;
        screen = true;
        break;
      case MsgType.offPhoneScreen:
        screen = false;
        break;
      default:
        break;
    }
  });
  void send(MsgType type) => transport.send(type, deviceMac: mac, data: Uint8List.fromList(mac.codeUnits));
  send(MsgType.onCamera);
  send(MsgType.onPhoneScreen);
  local.connected = true;
  send(MsgType.offCamera);
  send(MsgType.offPhoneScreen);
  check(!camera && !screen, 'LAN close left remote subscriptions enabled');
  check(local.sent.contains(MsgType.offCamera) && local.sent.contains(MsgType.offPhoneScreen), 'device did not receive LAN teardown');
  local.connected = false;
  send(MsgType.onCamera);
  send(MsgType.onPhoneScreen);
  check(cameraStarts == 2 && screenStarts == 2, 'remote reopen was suppressed by stale state');
  local.connected = true;
  final before = remote.length;
  send(MsgType.onCamera);
  check(remote.length == before, 'ordinary LAN messages duplicated remotely');
  local.connected = false;
  send(MsgType.offCamera);
  check(remote.length == before + 1, 'fallback teardown sent more than once');
  await local.dispose();
}

Future<void> addresses() async {
  final server = FakeServer();
  var current = <String>[];
  int binds = 0;
  final service = LocalAreaNetworkService.forTesting((value) => value,
      addressLookup: () async => current,
      bindListener: () async { binds++; return server; });
  final announced = <LanServerEndpoint?>[];
  final subscription = service.serverEndpoints.listen(announced.add);
  await service.setTargetDevice(mac);
  await service.retainForPage();
  check(service.serverEndpoint == null && binds == 1, 'initial lookup failure should keep listener');
  current = ['192.0.2.10'];
  await service.setTargetDevice(mac);
  check(service.serverEndpoint?.addresses.single == '192.0.2.10', 'lookup did not recover without a rebind');
  current = ['192.0.2.11'];
  await service.setTargetDevice(mac);
  check(service.serverEndpoint?.addresses.single == '192.0.2.11' && binds == 1 && !server.closed, 'IPv4 refresh rebound or retained stale endpoint');
  current = [];
  await service.setTargetDevice(mac);
  check(service.serverEndpoint == null && binds == 1, 'lost interfaces retained stale advertisement');
  current = ['192.0.2.12'];
  await service.setTargetDevice(mac);
  check(service.serverEndpoint?.addresses.single == '192.0.2.12', 'second lookup recovery failed');
  await Future<void>.delayed(Duration.zero);
  check(announced.whereType<LanServerEndpoint>().length == 3 && announced.contains(null), 'endpoint stream did not report changes/withdrawal');
  await service.releaseForPage();
  check(server.closed && service.serverEndpoint == null, 'page release did not stop listener');
  await service.dispose();
  await subscription.cancel();
}

Future<void> staleLookup() async {
  final pending = Completer<List<String>>();
  final started = Completer<void>();
  final server = FakeServer();
  final service = LocalAreaNetworkService.forTesting((value) => value,
      addressLookup: () { started.complete(); return pending.future; },
      bindListener: () async => server);
  final announced = <LanServerEndpoint?>[];
  final subscription = service.serverEndpoints.listen(announced.add);
  await service.setTargetDevice(mac);
  final opening = service.retainForPage();
  await started.future;
  final closing = service.setForeground(false);
  pending.complete(['192.0.2.99']);
  await opening;
  await closing;
  await Future<void>.delayed(Duration.zero);
  check(service.serverEndpoint == null && server.closed, 'late lookup advertised a stopped generation');
  check(announced.whereType<LanServerEndpoint>().isEmpty, 'late lookup transiently advertised a stopped generation');
  await service.dispose();
  await subscription.cancel();
}

Future<void> main() async {
  await routing();
  await addresses();
  await staleLookup();
  print('Production remote/LAN teardown, stable-listener IPv4 recovery and stale-generation checks passed');
}
