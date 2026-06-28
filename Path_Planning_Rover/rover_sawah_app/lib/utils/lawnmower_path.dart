import 'dart:math';
import 'package:latlong2/latlong.dart';

class LawnmowerPathGenerator {
  /// Generates a boustrophedon (lawnmower) path inside a given polygon.
  /// [polygon] must be a closed or open list of LatLng vertices (at least 3).
  /// [spacingMeters] is the distance between sweeps (rows).
  static List<LatLng> generate(List<LatLng> polygon, double spacingMeters) {
    if (polygon.length < 3) return [];

    // 1. Calculate Bounding Box
    double minLat = polygon[0].latitude;
    double maxLat = polygon[0].latitude;
    double minLng = polygon[0].longitude;
    double maxLng = polygon[0].longitude;

    for (var point in polygon) {
      if (point.latitude < minLat) minLat = point.latitude;
      if (point.latitude > maxLat) maxLat = point.latitude;
      if (point.longitude < minLng) minLng = point.longitude;
      if (point.longitude > maxLng) maxLng = point.longitude;
    }

    // Approx 1 degree latitude = 111,320 meters
    double latStep = spacingMeters / 111320.0;
    
    // Ensure we have a closed polygon for intersection testing
    List<LatLng> closedPoly = List.from(polygon);
    if (closedPoly.first != closedPoly.last) {
      closedPoly.add(closedPoly.first);
    }

    List<LatLng> path = [];
    bool goingRight = true;

    // Scan from top (North) to bottom (South)
    // Starting slightly inside the boundary
    double currentLat = maxLat - (latStep / 2);

    while (currentLat > minLat) {
      List<double> intersections = [];

      // Find intersections of the horizontal line `y = currentLat` with polygon edges
      for (int i = 0; i < closedPoly.length - 1; i++) {
        LatLng p1 = closedPoly[i];
        LatLng p2 = closedPoly[i + 1];

        // Check if the edge crosses the horizontal line
        if ((p1.latitude <= currentLat && p2.latitude > currentLat) ||
            (p2.latitude <= currentLat && p1.latitude > currentLat)) {
          
          // Calculate intersection longitude using linear interpolation
          double fraction = (currentLat - p1.latitude) / (p2.latitude - p1.latitude);
          double intersectLng = p1.longitude + fraction * (p2.longitude - p1.longitude);
          
          intersections.add(intersectLng);
        }
      }

      // Sort intersections from West to East
      intersections.sort();

      // Pair them up to form line segments inside the polygon
      // For standard convex/simple concave polygons, this works perfectly.
      List<LatLng> rowPoints = [];
      for (int i = 0; i < intersections.length - 1; i += 2) {
        rowPoints.add(LatLng(currentLat, intersections[i]));
        rowPoints.add(LatLng(currentLat, intersections[i + 1]));
      }

      if (rowPoints.isNotEmpty) {
        // Snake pattern logic
        if (!goingRight) {
          rowPoints = rowPoints.reversed.toList();
        }
        path.addAll(rowPoints);
        
        // Flip direction for next row
        goingRight = !goingRight;
      }

      currentLat -= latStep;
    }

    return path;
  }
}
