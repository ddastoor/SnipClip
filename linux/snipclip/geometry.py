"""Pure, display-independent geometry and hit-testing routines for SnipClip."""

from __future__ import annotations

import math
from typing import Sequence, Tuple


def normalize_rect_bounds(
    x1: float,
    y1: float,
    x2: float,
    y2: float,
    max_width: int,
    max_height: int,
) -> Tuple[int, int, int, int]:
    """Normalize two drag coordinates into clamped (x, y, width, height) integer bounds."""
    left = max(0.0, min(float(max_width), min(x1, x2)))
    top = max(0.0, min(float(max_height), min(y1, y2)))
    right = max(0.0, min(float(max_width), max(x1, x2)))
    bottom = max(0.0, min(float(max_height), max(y1, y2)))
    ix = int(round(left))
    iy = int(round(top))
    iw = int(round(right - left))
    ih = int(round(bottom - top))
    return (ix, iy, iw, ih)


def compute_polygon_bbox(
    points: Sequence[Tuple[float, float]],
    max_width: int,
    max_height: int,
    min_size: int = 8,
) -> Tuple[int, int, int, int] | None:
    """Compute clamped bounding box (x, y, width, height) for a freeform lasso path."""
    if len(points) < 3:
        return None
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    x1 = max(0, min(max_width, math.floor(min(xs))))
    y1 = max(0, min(max_height, math.floor(min(ys))))
    x2 = max(0, min(max_width, math.ceil(max(xs))))
    y2 = max(0, min(max_height, math.ceil(max(ys))))
    w = x2 - x1
    h = y2 - y1
    if w < min_size or h < min_size:
        return None
    return (x1, y1, w, h)


def fit_ruler_to_canvas(
    canvas_width: int,
    canvas_height: int,
) -> Tuple[float, float, float, float]:
    """Return (center_x, center_y, length, half_thickness) proportioned to the captured snip."""
    safe_w = max(80, canvas_width)
    safe_h = max(80, canvas_height)
    length = float(min(400, max(140, safe_w - 56)))
    half_thickness = float(min(28, max(18, int(safe_h * 0.1))))
    return (safe_w / 2.0, safe_h / 2.0, length, half_thickness)


def snap_point_to_ruler(
    px: float,
    py: float,
    ruler_cx: float,
    ruler_cy: float,
    angle_deg: float,
    length: float = 420.0,
    half_thickness: float = 28.0,
    snap_threshold: float = 18.0,
) -> Tuple[float, float]:
    """Project (px, py) onto the rotated ruler's top or bottom straight-edge if within threshold."""
    rad = math.radians(angle_deg)
    dx = math.cos(rad)
    dy = math.sin(rad)
    nx = -dy
    ny = dx

    rel_x = px - ruler_cx
    rel_y = py - ruler_cy
    along = rel_x * dx + rel_y * dy
    perp = rel_x * nx + rel_y * ny

    if abs(along) <= (length / 2.0 + 10.0):
        if abs(perp - half_thickness) <= snap_threshold:
            return (
                ruler_cx + along * dx + half_thickness * nx,
                ruler_cy + along * dy + half_thickness * ny,
            )
        if abs(perp + half_thickness) <= snap_threshold:
            return (
                ruler_cx + along * dx - half_thickness * nx,
                ruler_cy + along * dy - half_thickness * ny,
            )
    return (px, py)


def point_inside_ruler(
    px: float,
    py: float,
    ruler_cx: float,
    ruler_cy: float,
    angle_deg: float,
    length: float = 420.0,
    half_thickness: float = 26.0,
) -> bool:
    """Return True when (px, py) lies inside the draggable body of the rotated ruler."""
    rad = math.radians(angle_deg)
    dx = math.cos(rad)
    dy = math.sin(rad)
    nx = -dy
    ny = dx
    rel_x = px - ruler_cx
    rel_y = py - ruler_cy
    along = rel_x * dx + rel_y * dy
    perp = rel_x * nx + rel_y * ny
    return abs(along) <= length / 2.0 and abs(perp) <= half_thickness


def distance_point_to_segment(
    px: float,
    py: float,
    ax: float,
    ay: float,
    bx: float,
    by: float,
) -> float:
    """Minimum Euclidean distance between point P and line segment AB."""
    seg_len_sq = (bx - ax) ** 2 + (by - ay) ** 2
    if seg_len_sq == 0.0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * (bx - ax) + (py - ay) * (by - ay)) / seg_len_sq))
    proj_x = ax + t * (bx - ax)
    proj_y = ay + t * (by - ay)
    return math.hypot(px - proj_x, py - proj_y)


def hit_test_annotation_points(
    px: float,
    py: float,
    kind: str,
    points: Sequence[Tuple[float, float]],
    threshold: float = 14.0,
) -> bool:
    """Determine if an eraser cursor at (px, py) intersects an annotation stroke or shape."""
    if not points:
        return False
    if len(points) == 1:
        return math.hypot(px - points[0][0], py - points[0][1]) <= threshold

    if kind in ("pen", "highlighter"):
        for idx in range(len(points) - 1):
            ax, ay = points[idx]
            bx, by = points[idx + 1]
            if distance_point_to_segment(px, py, ax, ay, bx, by) <= threshold:
                return True
        return False

    x1, y1 = points[0]
    x2, y2 = points[-1]

    if kind in ("line", "arrow"):
        return distance_point_to_segment(px, py, x1, y1, x2, y2) <= threshold

    left = min(x1, x2) - threshold
    right = max(x1, x2) + threshold
    top = min(y1, y2) - threshold
    bottom = max(y1, y2) + threshold
    return left <= px <= right and top <= py <= bottom


def compute_arrow_wings(
    x1: float,
    y1: float,
    x2: float,
    y2: float,
    stroke_width: float,
) -> Tuple[Tuple[float, float], Tuple[float, float]]:
    """Compute the two arrowhead wing endpoints terminating at (x2, y2)."""
    angle = math.atan2(y2 - y1, x2 - x1)
    head_len = max(10.0, stroke_width * 3.5)
    left = (
        x2 - head_len * math.cos(angle - math.pi / 6.0),
        y2 - head_len * math.sin(angle - math.pi / 6.0),
    )
    right = (
        x2 - head_len * math.cos(angle + math.pi / 6.0),
        y2 - head_len * math.sin(angle + math.pi / 6.0),
    )
    return left, right
