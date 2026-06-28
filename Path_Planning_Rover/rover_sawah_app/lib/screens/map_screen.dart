import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:flutter_map_tile_caching/flutter_map_tile_caching.dart';
import 'package:latlong2/latlong.dart';
import 'package:provider/provider.dart';
import '../services/websocket_service.dart';
import '../utils/lawnmower_path.dart';

class MapScreen extends StatefulWidget {
  const MapScreen({super.key});

  @override
  State<MapScreen> createState() => _MapScreenState();
}

class _MapScreenState extends State<MapScreen> {
  final MapController _mapController = MapController();
  final List<LatLng> _roverTrail = [];
  bool _followRover = true;

  // Polygon Drawing State
  bool _isDrawingArea = false;
  List<LatLng> _polygonPoints = [];
  List<LatLng> _generatedPath = [];
  double _rowSpacingMeters = 2.0;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Rover Sawah Monitor'),
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        actions: [
          _buildConnectionStatus(),
        ],
      ),
      body: Stack(
        children: [
          _buildMap(),
          _buildBottomPanel(),
        ],
      ),
      floatingActionButton: Column(
        mainAxisAlignment: MainAxisAlignment.end,
        crossAxisAlignment: CrossAxisAlignment.end,
        children: [
          if (!_isDrawingArea && _polygonPoints.isEmpty)
            FloatingActionButton.extended(
              onPressed: () {
                setState(() {
                  _isDrawingArea = true;
                  _polygonPoints.clear();
                  _generatedPath.clear();
                  _followRover = false;
                });
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('Ketuk peta untuk mulai menggambar area sawah (Min. 3 titik)')),
                );
              },
              icon: const Icon(Icons.draw),
              label: const Text('Gambar Area Sawah'),
            ),
            
          if (_isDrawingArea) ...[
            FloatingActionButton.extended(
              heroTag: 'btnSelesai',
              backgroundColor: Colors.green,
              foregroundColor: Colors.white,
              onPressed: _polygonPoints.length >= 3 ? () {
                setState(() {
                  _isDrawingArea = false;
                });
              } : null,
              icon: const Icon(Icons.check),
              label: const Text('Selesai Gambar'),
            ),
            const SizedBox(height: 16),
            FloatingActionButton.extended(
              heroTag: 'btnReset',
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
              onPressed: () {
                setState(() {
                  _polygonPoints.clear();
                });
              },
              icon: const Icon(Icons.delete),
              label: const Text('Reset'),
            ),
          ],
        ],
      ),
      floatingActionButtonLocation: FloatingActionButtonLocation.endFloat,
    );
  }

  Widget _buildConnectionStatus() {
    return Consumer<RoverState>(
      builder: (context, state, child) {
        IconData icon;
        Color color;
        String text;

        switch (state.status) {
          case ConnectionStatus.connected:
            icon = Icons.wifi;
            color = Colors.green;
            text = 'Terhubung';
            break;
          case ConnectionStatus.connecting:
            icon = Icons.wifi_find;
            color = Colors.orange;
            text = 'Mencari...';
            break;
          case ConnectionStatus.disconnected:
            icon = Icons.wifi_off;
            color = Colors.red;
            text = 'Terputus';
            break;
        }

        return GestureDetector(
          onTap: () => _showIpDialog(context, state),
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12.0),
            child: Row(
              children: [
                Icon(icon, color: color, size: 20),
                const SizedBox(width: 6),
                Text(text, style: TextStyle(color: color, fontWeight: FontWeight.bold)),
                const SizedBox(width: 4),
                Icon(Icons.edit, color: color, size: 14),
              ],
            ),
          ),
        );
      },
    );
  }

  void _showIpDialog(BuildContext context, RoverState state) {
    final controller = TextEditingController(text: state.esp32Ip);
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Koneksi ke ESP32'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Masukkan IP ESP32 dari Serial Monitor:'),
            const SizedBox(height: 8),
            TextField(
              controller: controller,
              keyboardType: const TextInputType.numberWithOptions(decimal: true),
              decoration: const InputDecoration(
                hintText: 'Contoh: 10.35.18.138',
                border: OutlineInputBorder(),
                prefixText: 'ws://',
                suffixText: ':81',
              ),
            ),
            const SizedBox(height: 8),
            const Text(
              'IP ESP32 bisa dilihat di Serial Monitor Arduino setelah terhubung ke Hotspot HP.',
              style: TextStyle(fontSize: 12, color: Colors.grey),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Batal'),
          ),
          ElevatedButton(
            onPressed: () {
              Navigator.pop(ctx);
              final ip = controller.text.trim();
              if (ip.isNotEmpty) {
                state.connectToIp(ip);
              }
            },
            child: const Text('Hubungkan'),
          ),
        ],
      ),
    );
  }

  void _handleMapTap(TapPosition tapPosition, LatLng point) {
    if (_isDrawingArea) {
      setState(() {
        _polygonPoints.add(point);
      });
    }
  }

  Widget _buildMap() {
    return Consumer<RoverState>(
      builder: (context, state, child) {
        if (state.hasValidPosition) {
          if (_roverTrail.isEmpty || _roverTrail.last != state.roverPosition) {
            _roverTrail.add(state.roverPosition);
          }
          if (_followRover && !_isDrawingArea) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
              _mapController.move(state.roverPosition, _mapController.camera.zoom);
            });
          }
        }

        return FlutterMap(
          mapController: _mapController,
          options: MapOptions(
            initialCenter: const LatLng(-6.200000, 106.816666), // Default Jakarta
            initialZoom: 18.0,
            onTap: _handleMapTap,
            onPositionChanged: (position, hasGesture) {
              if (hasGesture && _followRover) {
                setState(() => _followRover = false);
              }
            },
          ),
          children: [
            TileLayer(
              urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
              userAgentPackageName: 'com.example.roversawah',
              tileProvider: FMTCStore('rover_map_store').getTileProvider(),
            ),
            
            // Area Sawah Polygon
            if (_polygonPoints.isNotEmpty)
              PolygonLayer(
                polygons: [
                  Polygon(
                    points: _polygonPoints,
                    color: Colors.green.withOpacity(0.3),
                    borderColor: Colors.green,
                    borderStrokeWidth: 2.0,
                    isFilled: true,
                  ),
                ],
              ),
              
            // Polygon Drawing Points (Vertices)
            if (_isDrawingArea || _polygonPoints.isNotEmpty)
              MarkerLayer(
                markers: _polygonPoints.map((p) => Marker(
                  point: p,
                  width: 10,
                  height: 10,
                  child: const CircleAvatar(backgroundColor: Colors.green),
                )).toList(),
              ),

            // Generated Lawnmower Path
            if (_generatedPath.isNotEmpty)
              PolylineLayer(
                polylines: [
                  Polyline(
                    points: _generatedPath,
                    color: Colors.yellow,
                    strokeWidth: 3.0,
                    isDotted: true,
                  ),
                ],
              ),
              
            // Generated Waypoints (Dots)
            if (_generatedPath.isNotEmpty)
              MarkerLayer(
                markers: _generatedPath.map((p) => Marker(
                  point: p,
                  width: 8,
                  height: 8,
                  child: const CircleAvatar(backgroundColor: Colors.orange),
                )).toList(),
              ),

            // Trail History
            PolylineLayer(
              polylines: [
                Polyline(
                  points: _roverTrail,
                  color: Colors.blue,
                  strokeWidth: 4.0,
                ),
              ],
            ),
            
            // Realtime Rover Position
            MarkerLayer(
              markers: [
                if (state.hasValidPosition)
                  Marker(
                    point: state.roverPosition,
                    width: 40,
                    height: 40,
                    child: const Icon(
                      Icons.agriculture, // Tractor/rover icon
                      color: Colors.red,
                      size: 40,
                    ),
                  ),
              ],
            ),
          ],
        );
      },
    );
  }

  Widget _buildBottomPanel() {
    return Consumer<RoverState>(
      builder: (context, state, child) {
        return Positioned(
          bottom: 0,
          left: 0,
          right: 0,
          child: Container(
            padding: const EdgeInsets.all(16.0),
            decoration: BoxDecoration(
              color: Colors.white.withOpacity(0.95),
              boxShadow: const [
                BoxShadow(
                  color: Colors.black12,
                  blurRadius: 10,
                  offset: Offset(0, -2),
                )
              ],
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              mainAxisSize: MainAxisSize.min,
              children: [
                // Info Section
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      'Koordinat Rover:',
                      style: Theme.of(context).textTheme.titleMedium,
                    ),
                    if (!_followRover && !_isDrawingArea)
                      TextButton.icon(
                        icon: const Icon(Icons.my_location),
                        label: const Text('Ikuti Rover'),
                        onPressed: () {
                          setState(() => _followRover = true);
                          if (state.hasValidPosition) {
                            _mapController.move(state.roverPosition, 18.0);
                          }
                        },
                      )
                  ],
                ),
                if (state.hasValidPosition)
                  Text(
                    'Lat: ${state.roverPosition.latitude.toStringAsFixed(6)}, Lng: ${state.roverPosition.longitude.toStringAsFixed(6)}',
                    style: const TextStyle(fontSize: 16, fontFamily: 'monospace'),
                  )
                else
                  const Text('Menunggu data GPS...'),
                  
                if (state.lastAckStatus != null) ...[
                  const Divider(),
                  Text(
                    'Status Terakhir: ${state.lastAckStatus}',
                    style: TextStyle(
                      fontWeight: FontWeight.bold,
                      color: state.lastAckStatus == 'OK' ? Colors.green : Colors.red,
                    ),
                  ),
                ],
                
                // Lawnmower Controls
                if (!_isDrawingArea && _polygonPoints.isNotEmpty) ...[
                  const Divider(),
                  Row(
                    children: [
                      const Text('Spasi Jalur (M): '),
                      Expanded(
                        child: Slider(
                          value: _rowSpacingMeters,
                          min: 1,
                          max: 10,
                          divisions: 18,
                          label: '${_rowSpacingMeters.toStringAsFixed(1)}m',
                          onChanged: (val) {
                            setState(() {
                              _rowSpacingMeters = val;
                            });
                          },
                          onChangeEnd: (val) {
                            _generatePath();
                          },
                        ),
                      ),
                      Text('${_rowSpacingMeters.toStringAsFixed(1)}m'),
                    ],
                  ),
                  
                  if (_generatedPath.isNotEmpty) ...[
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Text('Total Waypoint: ${_generatedPath.length}'),
                        Text('Est. Jarak: ${_calculateDistance().toStringAsFixed(1)} m'),
                      ],
                    ),
                    const SizedBox(height: 12),
                    ElevatedButton(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.blue,
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(vertical: 12),
                      ),
                      onPressed: () {
                        context.read<RoverState>().sendPath(_generatedPath);
                        ScaffoldMessenger.of(context).showSnackBar(
                          SnackBar(content: Text('Mengirim ${_generatedPath.length} waypoint ke Rover...')),
                        );
                      },
                      child: const Text('Kirim ke Rover'),
                    ),
                  ] else ...[
                    ElevatedButton(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.orange,
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(vertical: 12),
                      ),
                      onPressed: _generatePath,
                      child: const Text('Generate Path'),
                    ),
                  ],
                ],
              ],
            ),
          ),
        );
      },
    );
  }

  void _generatePath() {
    if (_polygonPoints.length < 3) return;
    
    final path = LawnmowerPathGenerator.generate(_polygonPoints, _rowSpacingMeters);
    setState(() {
      _generatedPath = path;
    });
  }

  double _calculateDistance() {
    if (_generatedPath.length < 2) return 0;
    final Distance distance = const Distance();
    double total = 0;
    for (int i = 0; i < _generatedPath.length - 1; i++) {
      total += distance.as(LengthUnit.Meter, _generatedPath[i], _generatedPath[i+1]);
    }
    return total;
  }
}
