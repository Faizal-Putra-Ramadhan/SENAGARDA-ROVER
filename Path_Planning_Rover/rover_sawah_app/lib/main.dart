import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:flutter_map_tile_caching/flutter_map_tile_caching.dart';
import 'services/websocket_service.dart';
import 'screens/map_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  // Initialize Flutter Map Tile Caching
  try {
    await FMTCObjectBoxBackend().initialise();
    final store = FMTCStore('rover_map_store');
    await store.manage.create();
  } catch (e) {
    debugPrint("FMTC Initialization Error: $e");
  }

  runApp(const RoverSawahApp());
}

class RoverSawahApp extends StatelessWidget {
  const RoverSawahApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => RoverState()..connect()),
      ],
      child: MaterialApp(
        title: 'Rover Sawah',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          colorScheme: ColorScheme.fromSeed(
            seedColor: Colors.green,
            brightness: Brightness.light,
          ),
          useMaterial3: true,
        ),
        home: const MapScreen(),
      ),
    );
  }
}
