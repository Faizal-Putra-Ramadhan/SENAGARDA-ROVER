import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:latlong2/latlong.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:multicast_dns/multicast_dns.dart';

enum ConnectionStatus { disconnected, connecting, connected }

class RoverState extends ChangeNotifier {
  WebSocketChannel? _channel;
  ConnectionStatus _status = ConnectionStatus.disconnected;
  LatLng _roverPosition = const LatLng(0, 0);
  bool _hasValidPosition = false;
  String? _lastAckStatus;
  String _esp32Ip = ''; // IP akan di-resolve, atau di-set manual
  
  // Timer for auto-reconnect
  Timer? _reconnectTimer;
  final String _mdnsHostname = 'rover-gateway.local';

  ConnectionStatus get status => _status;
  LatLng get roverPosition => _roverPosition;
  bool get hasValidPosition => _hasValidPosition;
  String? get lastAckStatus => _lastAckStatus;
  String get esp32Ip => _esp32Ip;

  /// Digunakan dari UI untuk menghubungkan ke IP tertentu secara manual
  void connectToIp(String ip) {
    _reconnectTimer?.cancel();
    _channel?.sink.close();
    _status = ConnectionStatus.disconnected;
    _esp32Ip = ip;
    notifyListeners();
    _connectToUrl('ws://$ip:81');
  }

  Future<String?> _resolveMDNS() async {
    final MDnsClient client = MDnsClient();
    await client.start();
    
    String? resolvedIp;
    try {
      debugPrint("Mencari mDNS untuk $_mdnsHostname...");
      await for (final IPAddressResourceRecord record in client
          .lookup<IPAddressResourceRecord>(
              ResourceRecordQuery.addressIPv4(_mdnsHostname))
          .timeout(const Duration(seconds: 4), onTimeout: (sink) => sink.close())) {
        resolvedIp = record.address.address;
        debugPrint("mDNS ditemukan: $resolvedIp");
        break;
      }
    } catch (e) {
      debugPrint("mDNS gagal: $e");
    } finally {
      client.stop();
    }
    return resolvedIp;
  }

  void connect() async {
    if (_status == ConnectionStatus.connected || _status == ConnectionStatus.connecting) return;
    
    _status = ConnectionStatus.connecting;
    notifyListeners();

    // Jika sudah ada IP yang di-set manual, langsung pakai
    if (_esp32Ip.isNotEmpty) {
      _connectToUrl('ws://$_esp32Ip:81');
      return;
    }

    // Coba resolve mDNS dulu
    String? ip = await _resolveMDNS();
    if (ip != null) {
      _esp32Ip = ip;
      notifyListeners();
      _connectToUrl('ws://$ip:81');
    } else {
      // mDNS gagal, set status disconnect agar UI bisa minta input IP manual
      debugPrint("mDNS gagal. Butuh input IP manual.");
      _status = ConnectionStatus.disconnected;
      notifyListeners();
    }
  }

  void _connectToUrl(String wsUrl) {
    try {
      debugPrint("Menghubungkan ke $wsUrl");
      _channel = WebSocketChannel.connect(Uri.parse(wsUrl));
      _status = ConnectionStatus.connected;
      notifyListeners();

      _channel!.stream.listen(
        (message) => _handleMessage(message),
        onDone: () => _handleDisconnect(),
        onError: (error) => _handleDisconnect(),
      );
    } catch (e) {
      debugPrint("Gagal konek WebSocket: $e");
      _handleDisconnect();
    }
  }

  void _handleDisconnect() {
    if (_status != ConnectionStatus.disconnected) {
      _status = ConnectionStatus.disconnected;
      notifyListeners();
    }
    _reconnectTimer?.cancel();
    // Hanya auto-reconnect jika sudah ada IP yang diketahui
    if (_esp32Ip.isNotEmpty) {
      _reconnectTimer = Timer(const Duration(seconds: 5), () => connect());
    }
  }

  void _handleMessage(dynamic message) {
    try {
      final data = jsonDecode(message);
      if (data['type'] == 'gps') {
        _roverPosition = LatLng((data['lat'] as num).toDouble(), (data['lng'] as num).toDouble());
        _hasValidPosition = true;
        notifyListeners();
      } else if (data['type'] == 'ack') {
        _lastAckStatus = data['status'];
        notifyListeners();
        Future.delayed(const Duration(seconds: 3), () {
          _lastAckStatus = null;
          notifyListeners();
        });
      }
    } catch (e) {
      debugPrint("Error parsing WebSocket: $e");
    }
  }

  void sendPath(List<LatLng> waypoints) {
    if (_status != ConnectionStatus.connected || _channel == null) return;
    final message = jsonEncode({
      'type': 'path',
      'waypoints': waypoints.map((wp) => {'lat': wp.latitude, 'lng': wp.longitude}).toList(),
    });
    _channel!.sink.add(message);
    _lastAckStatus = "Menunggu balasan...";
    notifyListeners();
  }

  @override
  void dispose() {
    _reconnectTimer?.cancel();
    _channel?.sink.close();
    super.dispose();
  }
}
