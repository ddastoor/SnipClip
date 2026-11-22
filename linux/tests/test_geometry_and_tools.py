"""Extensive unit tests for SnipClip pure geometry, ruler straight-edge snapping, and stroke hit-testing."""

from __future__ import annotations

import math
import sys
import unittest
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT_DIR / "linux"))

from snipclip import geometry  # noqa: E402


class GeometryAndAnnotationMathTests(unittest.TestCase):
    def test_normalize_rect_bounds_handles_inverted_and_out_of_bounds_drags(self) -> None:
        # Inverted drag from bottom-right to top-left going past negative top edge
        rect = geometry.normalize_rect_bounds(420.0, 310.0, 100.0, -25.0, 1920, 1080)
        self.assertEqual(rect, (100, 0, 320, 310))

        # Drag exceeding bottom-right monitor boundary
        clamped = geometry.normalize_rect_bounds(1800.0, 1000.0, 2100.0, 1250.0, 1920, 1080)
        self.assertEqual(clamped, (1800, 1000, 120, 80))

    def test_compute_polygon_bbox_validates_point_count_and_minimum_dimensions(self) -> None:
        self.assertIsNone(geometry.compute_polygon_bbox([(10.0, 10.0), (50.0, 50.0)], 800, 600))
        self.assertIsNone(
            geometry.compute_polygon_bbox([(10.0, 10.0), (14.0, 11.0), (12.0, 15.0)], 800, 600)
        )

        bbox = geometry.compute_polygon_bbox(
            [(-10.0, 25.2), (180.7, 40.0), (95.0, 220.4)], 800, 600
        )
        self.assertEqual(bbox, (0, 25, 181, 196))

    def test_fit_ruler_to_canvas_stays_within_narrow_captures(self) -> None:
        cx, cy, length, half_t = geometry.fit_ruler_to_canvas(240, 180)
        self.assertEqual((cx, cy), (120.0, 90.0))
        self.assertLessEqual(length, 240.0 - 40.0)
        self.assertGreaterEqual(half_t, 18.0)

    def test_ruler_snapping_horizontal_top_and_bottom_edges(self) -> None:
        # Ruler centered at (300, 200), angle 0 deg -> edges at y = 172 and y = 228
        bx, by = geometry.snap_point_to_ruler(340.0, 236.0, 300.0, 200.0, 0.0)
        self.assertAlmostEqual(bx, 340.0, places=4)
        self.assertAlmostEqual(by, 228.0, places=4)

        tx, ty = geometry.snap_point_to_ruler(260.0, 165.0, 300.0, 200.0, 0.0)
        self.assertAlmostEqual(tx, 260.0, places=4)
        self.assertAlmostEqual(ty, 172.0, places=4)

        # Point far beyond ruler length is not snapped
        fx, fy = geometry.snap_point_to_ruler(650.0, 232.0, 300.0, 200.0, 0.0)
        self.assertEqual((fx, fy), (650.0, 232.0))

    def test_ruler_snapping_vertical_90_degrees(self) -> None:
        # Ruler centered at (300, 200), angle 90 deg -> edges at x = 272 and x = 328
        sx, sy = geometry.snap_point_to_ruler(266.0, 250.0, 300.0, 200.0, 90.0)
        self.assertAlmostEqual(sx, 272.0, places=4)
        self.assertAlmostEqual(sy, 250.0, places=4)

    def test_ruler_snapping_diagonal_45_degrees_lies_on_edge_line(self) -> None:
        cx, cy = 400.0, 300.0
        angle = 45.0
        rad = math.radians(angle)
        nx, ny = -math.sin(rad), math.cos(rad)
        # Place a point 33px perpendicular to center (5px outside the 28px half-thickness)
        px = cx + 50.0 * math.cos(rad) + 33.0 * nx
        py = cy + 50.0 * math.sin(rad) + 33.0 * ny
        sx, sy = geometry.snap_point_to_ruler(px, py, cx, cy, angle)
        perp_after = (sx - cx) * nx + (sy - cy) * ny
        self.assertAlmostEqual(perp_after, 28.0, places=4)

    def test_point_inside_ruler_body_detection(self) -> None:
        self.assertTrue(geometry.point_inside_ruler(310.0, 205.0, 300.0, 200.0, 0.0))
        self.assertFalse(geometry.point_inside_ruler(310.0, 260.0, 300.0, 200.0, 0.0))
        self.assertFalse(geometry.point_inside_ruler(600.0, 200.0, 300.0, 200.0, 0.0))

    def test_eraser_hit_testing_catches_midpoints_of_arrows_lines_and_polylines(self) -> None:
        # Long horizontal arrow from (50, 100) to (450, 100): clicking midpoint (250, 106) must hit!
        arrow_pts = [(50.0, 100.0), (450.0, 100.0)]
        self.assertTrue(
            geometry.hit_test_annotation_points(250.0, 106.0, "arrow", arrow_pts, threshold=12.0)
        )
        self.assertFalse(
            geometry.hit_test_annotation_points(250.0, 140.0, "arrow", arrow_pts, threshold=12.0)
        )

        # Multi-segment freehand pen stroke
        pen_pts = [(10.0, 10.0), (100.0, 10.0), (100.0, 120.0)]
        self.assertTrue(
            geometry.hit_test_annotation_points(103.0, 65.0, "pen", pen_pts, threshold=10.0)
        )
        self.assertFalse(
            geometry.hit_test_annotation_points(50.0, 65.0, "pen", pen_pts, threshold=10.0)
        )

        # Redact / rectangle shape interior & boundary hit
        rect_pts = [(200.0, 200.0), (320.0, 280.0)]
        self.assertTrue(
            geometry.hit_test_annotation_points(260.0, 240.0, "redact", rect_pts, threshold=10.0)
        )
        self.assertFalse(
            geometry.hit_test_annotation_points(150.0, 240.0, "redact", rect_pts, threshold=10.0)
        )

    def test_compute_arrow_wings_symmetry(self) -> None:
        left, right = geometry.compute_arrow_wings(0.0, 0.0, 100.0, 0.0, 4.0)
        # Arrow pointing straight right (+X): wings should have identical X < 100 and opposite Y
        self.assertLess(left[0], 100.0)
        self.assertAlmostEqual(left[0], right[0], places=5)
        self.assertAlmostEqual(left[1], -right[1], places=5)


if __name__ == "__main__":
    unittest.main()
