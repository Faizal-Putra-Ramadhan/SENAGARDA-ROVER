import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:provider/provider.dart';
import '../services/websocket_service.dart';

class PathPlanningModal extends StatefulWidget {
  final LatLngBounds bounds;

  const PathPlanningModal({super.key, required this.bounds});

  @override
  State<PathPlanningModal> createState() => _PathPlanningModalState();
}

class _PathPlanningModalState extends State<PathPlanningModal> {
  int _gridSize = 3;
  Set<int> _selectedCells = {};

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: const BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
      ),
      padding: const EdgeInsets.all(24),
      height: MediaQuery.of(context).size.height * 0.7,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text('Buat Path Planning', style: Theme.of(context).textTheme.headlineSmall),
              IconButton(
                icon: const Icon(Icons.close),
                onPressed: () => Navigator.pop(context),
              )
            ],
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              const Text('Ukuran Grid: '),
              Expanded(
                child: Slider(
                  value: _gridSize.toDouble(),
                  min: 2,
                  max: 10,
                  divisions: 8,
                  label: '$_gridSize x $_gridSize',
                  onChanged: (val) {
                    setState(() {
                      _gridSize = val.toInt();
                      _selectedCells.clear();
                    });
                  },
                ),
              ),
              Text('$_gridSize x $_gridSize'),
            ],
          ),
          const SizedBox(height: 16),
          const Text('Ketuk sel untuk memilih area yang akan dilalui rover:'),
          const SizedBox(height: 8),
          Expanded(
            child: Container(
              decoration: BoxDecoration(
                border: Border.all(color: Colors.grey),
                color: Colors.grey[200],
              ),
              child: GridView.builder(
                physics: const NeverScrollableScrollPhysics(),
                gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                  crossAxisCount: _gridSize,
                  childAspectRatio: 1.0,
                ),
                itemCount: _gridSize * _gridSize,
                itemBuilder: (context, index) {
                  final isSelected = _selectedCells.contains(index);
                  return GestureDetector(
                    onTap: () {
                      setState(() {
                        if (isSelected) {
                          _selectedCells.remove(index);
                        } else {
                          _selectedCells.add(index);
                        }
                      });
                    },
                    child: Container(
                      margin: const EdgeInsets.all(2),
                      decoration: BoxDecoration(
                        color: isSelected ? Colors.green.withOpacity(0.7) : Colors.white,
                        border: Border.all(color: Colors.grey.shade300),
                      ),
                      child: Center(
                        child: Text(
                          isSelected ? '${_getSnakeSequenceIndex(index) + 1}' : '',
                          style: const TextStyle(fontWeight: FontWeight.bold, color: Colors.white),
                        ),
                      ),
                    ),
                  );
                },
              ),
            ),
          ),
          const SizedBox(height: 16),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              padding: const EdgeInsets.all(16),
              backgroundColor: Theme.of(context).primaryColor,
              foregroundColor: Colors.white,
            ),
            onPressed: _selectedCells.isEmpty ? null : _sendPath,
            child: Text('Kirim ke Rover (${_selectedCells.length} Waypoint)'),
          ),
        ],
      ),
    );
  }

  int _getSnakeSequenceIndex(int index) {
    // Generate the full snake sequence
    List<int> sequence = [];
    for (int r = 0; r < _gridSize; r++) {
      for (int c = 0; c < _gridSize; c++) {
        int cellIndex = (r % 2 == 0) ? (r * _gridSize + c) : (r * _gridSize + (_gridSize - 1 - c));
        if (_selectedCells.contains(cellIndex)) {
          sequence.add(cellIndex);
        }
      }
    }
    return sequence.indexOf(index);
  }

  void _sendPath() {
    List<LatLng> waypoints = [];
    
    double latDiff = (widget.bounds.northEast.latitude - widget.bounds.southWest.latitude).abs() / _gridSize;
    double lngDiff = (widget.bounds.northEast.longitude - widget.bounds.southWest.longitude).abs() / _gridSize;
    
    // Snake pattern logic (row by row, alternating direction)
    for (int r = 0; r < _gridSize; r++) {
      for (int c = 0; c < _gridSize; c++) {
        // Calculate cell index based on snake pattern
        int cellIndex = (r % 2 == 0) ? (r * _gridSize + c) : (r * _gridSize + (_gridSize - 1 - c));
        
        if (_selectedCells.contains(cellIndex)) {
          // Calculate lat/lng for center of the cell
          int actualRow = cellIndex ~/ _gridSize;
          int actualCol = cellIndex % _gridSize;
          
          double cellLat = widget.bounds.northWest.latitude - (actualRow * latDiff) - (latDiff / 2);
          double cellLng = widget.bounds.northWest.longitude + (actualCol * lngDiff) + (lngDiff / 2);
          
          waypoints.add(LatLng(cellLat, cellLng));
        }
      }
    }
    
    // Send to WebSocket
    context.read<RoverState>().sendPath(waypoints);
    
    Navigator.pop(context); // Close modal
    
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('Mengirim ${waypoints.length} waypoint ke Rover...')),
    );
  }
}
