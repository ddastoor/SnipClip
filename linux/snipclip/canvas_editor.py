"""Interactive Snipping Tool annotation canvas (Pen, Highlighter, Shapes, Redact, Ruler, Crop)."""

from __future__ import annotations

import dataclasses
import math
from typing import List, Optional, Tuple

from PyQt6.QtCore import QPointF, QRect, QRectF, Qt, pyqtSignal
from PyQt6.QtGui import (
    QColor,
    QFont,
    QImage,
    QMouseEvent,
    QPainter,
    QPainterPath,
    QPen,
    QPixmap,
    QWheelEvent,
)
from PyQt6.QtWidgets import QWidget

from .geometry import (
    compute_arrow_wings,
    hit_test_annotation_points,
    point_inside_ruler,
    snap_point_to_ruler,
)


@dataclasses.dataclass
class AnnotationItem:
    """Single vector or raster annotation operation on the snipping canvas."""

    kind: str  # pen | highlighter | rect | ellipse | line | arrow | redact | pixelate
    color: str
    width: float
    points: List[Tuple[float, float]]


@dataclasses.dataclass
class CanvasSnapshot:
    """Undo/Redo state snapshot preserving both base pixmap (for crops) and annotations."""

    pixmap: QPixmap
    items: List[AnnotationItem]


class SnipCanvasEditor(QWidget):
    """Canvas widget supporting Windows 11 Snipping Tool ink, shapes, ruler, redact, and crop."""

    history_changed = pyqtSignal(bool, bool)
    dimensions_changed = pyqtSignal(int, int)
    zoom_changed = pyqtSignal(float)
    crop_mode_changed = pyqtSignal(bool)

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._pixmap = QPixmap()
        self._items: List[AnnotationItem] = []
        self._active_item: Optional[AnnotationItem] = None

        self._undo_stack: List[CanvasSnapshot] = []
        self._redo_stack: List[CanvasSnapshot] = []

        self._tool = "pen"
        self._color = "#ef4444"
        self._stroke_width = 3.0
        self._zoom = 1.0

        # Ruler state (draggable + mouse-wheel rotatable straight-edge)
        self._ruler_visible = False
        self._ruler_center = QPointF(260.0, 180.0)
        self._ruler_angle_deg = 0.0
        self._dragging_ruler = False
        self._ruler_drag_offset = QPointF()

        # Crop state
        self._crop_active = False
        self._crop_rect = QRectF()
        self._dragging_crop = False
        self._crop_start = QPointF()

        self.setMouseTracking(True)
        self.setMinimumSize(480, 320)

    def has_image(self) -> bool:
        return not self._pixmap.isNull()

    def image_size(self) -> Tuple[int, int]:
        if self._pixmap.isNull():
            return (0, 0)
        return (self._pixmap.width(), self._pixmap.height())

    def set_pixmap(self, pixmap: QPixmap) -> None:
        self._pixmap = pixmap.copy()
        self._items.clear()
        self._undo_stack.clear()
        self._redo_stack.clear()
        self._crop_active = False
        self._zoom = 1.0
        self._ruler_center = QPointF(
            max(120.0, pixmap.width() / 2.0),
            max(90.0, pixmap.height() / 2.0),
        )
        self._sync_minimum_size()
        self.history_changed.emit(False, False)
        self.crop_mode_changed.emit(False)
        self.dimensions_changed.emit(self._pixmap.width(), self._pixmap.height())
        self.zoom_changed.emit(self._zoom)
        self.update()

    def set_tool(self, tool: str) -> None:
        self._tool = tool
        if tool == "crop" and not self._pixmap.isNull():
            self._crop_active = True
            margin_x = max(12.0, self._pixmap.width() * 0.08)
            margin_y = max(12.0, self._pixmap.height() * 0.08)
            self._crop_rect = QRectF(
                margin_x,
                margin_y,
                max(24.0, self._pixmap.width() - 2 * margin_x),
                max(24.0, self._pixmap.height() - 2 * margin_y),
            )
            self.crop_mode_changed.emit(True)
        elif self._crop_active and tool != "crop":
            self._crop_active = False
            self.crop_mode_changed.emit(False)
        self.update()

    def set_color(self, hex_color: str) -> None:
        self._color = hex_color

    def set_stroke_width(self, width: float) -> None:
        self._stroke_width = max(1.0, min(32.0, width))

    def toggle_ruler(self, visible: bool) -> None:
        self._ruler_visible = visible
        self.update()

    def set_zoom(self, zoom: float) -> None:
        self._zoom = max(0.25, min(4.0, zoom))
        self._sync_minimum_size()
        self.zoom_changed.emit(self._zoom)
        self.update()

    def zoom_factor(self) -> float:
        return self._zoom

    def _sync_minimum_size(self) -> None:
        if self._pixmap.isNull():
            self.setMinimumSize(480, 320)
            return
        w = int(self._pixmap.width() * self._zoom) + 64
        h = int(self._pixmap.height() * self._zoom) + 64
        self.setMinimumSize(max(480, w), max(320, h))

    def _push_undo(self) -> None:
        copied_items = [
            AnnotationItem(i.kind, i.color, i.width, list(i.points)) for i in self._items
        ]
        self._undo_stack.append(CanvasSnapshot(self._pixmap.copy(), copied_items))
        if len(self._undo_stack) > 40:
            self._undo_stack.pop(0)
        self._redo_stack.clear()
        self.history_changed.emit(bool(self._undo_stack), bool(self._redo_stack))

    def undo(self) -> None:
        if not self._undo_stack:
            return
        current = CanvasSnapshot(
            self._pixmap.copy(),
            [AnnotationItem(i.kind, i.color, i.width, list(i.points)) for i in self._items],
        )
        self._redo_stack.append(current)
        prev = self._undo_stack.pop()
        self._pixmap = prev.pixmap
        self._items = prev.items
        self._sync_minimum_size()
        self.dimensions_changed.emit(self._pixmap.width(), self._pixmap.height())
        self.history_changed.emit(bool(self._undo_stack), bool(self._redo_stack))
        self.update()

    def redo(self) -> None:
        if not self._redo_stack:
            return
        current = CanvasSnapshot(
            self._pixmap.copy(),
            [AnnotationItem(i.kind, i.color, i.width, list(i.points)) for i in self._items],
        )
        self._undo_stack.append(current)
        nxt = self._redo_stack.pop()
        self._pixmap = nxt.pixmap
        self._items = nxt.items
        self._sync_minimum_size()
        self.dimensions_changed.emit(self._pixmap.width(), self._pixmap.height())
        self.history_changed.emit(bool(self._undo_stack), bool(self._redo_stack))
        self.update()

    def clear_all_ink(self) -> None:
        if not self._items:
            return
        self._push_undo()
        self._items.clear()
        self.update()

    def apply_crop(self) -> None:
        if not self._crop_active or self._pixmap.isNull():
            return
        rendered = self.render_final_pixmap()
        crop_aligned = self._crop_rect.normalized().toAlignedRect().intersected(rendered.rect())
        if crop_aligned.width() < 8 or crop_aligned.height() < 8:
            return
        self._push_undo()
        self._pixmap = rendered.copy(crop_aligned)
        self._items.clear()
        self._crop_active = False
        self._tool = "pen"
        self._sync_minimum_size()
        self.crop_mode_changed.emit(False)
        self.dimensions_changed.emit(self._pixmap.width(), self._pixmap.height())
        self.update()

    def cancel_crop(self) -> None:
        self._crop_active = False
        if self._tool == "crop":
            self._tool = "pen"
        self.crop_mode_changed.emit(False)
        self.update()

    def render_final_pixmap(self) -> QPixmap:
        """Flatten the base snip and all annotations into an exportable QPixmap."""
        if self._pixmap.isNull():
            return QPixmap()
        result = self._pixmap.copy()
        painter = QPainter(result)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        for item in self._items:
            self._draw_annotation_item(painter, item, result)
        painter.end()
        return result

    def _canvas_origin(self) -> QPointF:
        if self._pixmap.isNull():
            return QPointF(0.0, 0.0)
        scaled_w = self._pixmap.width() * self._zoom
        scaled_h = self._pixmap.height() * self._zoom
        ox = max(24.0, (self.width() - scaled_w) / 2.0)
        oy = max(24.0, (self.height() - scaled_h) / 2.0)
        return QPointF(ox, oy)

    def _widget_to_image(self, pt: QPointF) -> QPointF:
        origin = self._canvas_origin()
        return QPointF((pt.x() - origin.x()) / self._zoom, (pt.y() - origin.y()) / self._zoom)

    def _snap_to_ruler_if_close(self, pt: QPointF) -> QPointF:
        if not self._ruler_visible:
            return pt
        sx, sy = snap_point_to_ruler(
            pt.x(),
            pt.y(),
            self._ruler_center.x(),
            self._ruler_center.y(),
            self._ruler_angle_deg,
            length=420.0,
            half_thickness=28.0,
            snap_threshold=18.0,
        )
        return QPointF(sx, sy)

    def _hit_tests_ruler(self, img_pt: QPointF) -> bool:
        if not self._ruler_visible:
            return False
        return point_inside_ruler(
            img_pt.x(),
            img_pt.y(),
            self._ruler_center.x(),
            self._ruler_center.y(),
            self._ruler_angle_deg,
            length=420.0,
            half_thickness=26.0,
        )

    def wheelEvent(self, event: QWheelEvent) -> None:
        if self._ruler_visible:
            img_pt = self._widget_to_image(event.position())
            if self._hit_tests_ruler(img_pt):
                delta = 5.0 if event.angleDelta().y() > 0 else -5.0
                self._ruler_angle_deg = (self._ruler_angle_deg + delta) % 360.0
                self.update()
                event.accept()
                return
        if event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            step = 0.1 if event.angleDelta().y() > 0 else -0.1
            self.set_zoom(round(self._zoom + step, 2))
            event.accept()
            return
        super().wheelEvent(event)

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if self._pixmap.isNull() or event.button() != Qt.MouseButton.LeftButton:
            return

        img_pt = self._widget_to_image(event.position())

        if self._hit_tests_ruler(img_pt):
            self._dragging_ruler = True
            self._ruler_drag_offset = img_pt - self._ruler_center
            return

        if self._crop_active:
            self._dragging_crop = True
            self._crop_start = img_pt
            self._crop_rect = QRectF(img_pt, img_pt)
            self.update()
            return

        if self._tool == "eraser":
            self._erase_near(img_pt)
            return

        snapped = self._snap_to_ruler_if_close(img_pt)
        width = self._stroke_width * (4.5 if self._tool == "highlighter" else 1.0)
        self._active_item = AnnotationItem(
            kind=self._tool,
            color=self._color,
            width=width,
            points=[(snapped.x(), snapped.y())],
        )
        self.update()

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        if self._pixmap.isNull():
            return
        img_pt = self._widget_to_image(event.position())

        if self._dragging_ruler:
            self._ruler_center = img_pt - self._ruler_drag_offset
            self.update()
            return

        if self._dragging_crop and self._crop_active:
            clamped = QPointF(
                max(0.0, min(float(self._pixmap.width()), img_pt.x())),
                max(0.0, min(float(self._pixmap.height()), img_pt.y())),
            )
            self._crop_rect = QRectF(self._crop_start, clamped).normalized()
            self.update()
            return

        if event.buttons() & Qt.MouseButton.LeftButton and self._tool == "eraser":
            self._erase_near(img_pt)
            return

        if self._active_item is not None:
            snapped = self._snap_to_ruler_if_close(img_pt)
            if self._active_item.kind in ("pen", "highlighter"):
                self._active_item.points.append((snapped.x(), snapped.y()))
            else:
                if len(self._active_item.points) == 1:
                    self._active_item.points.append((snapped.x(), snapped.y()))
                else:
                    self._active_item.points[1] = (snapped.x(), snapped.y())
            self.update()

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if event.button() != Qt.MouseButton.LeftButton:
            return
        if self._dragging_ruler:
            self._dragging_ruler = False
            return
        if self._dragging_crop:
            self._dragging_crop = False
            return
        if self._active_item is not None:
            if len(self._active_item.points) >= 2 or self._active_item.kind in ("pen", "highlighter"):
                self._push_undo()
                self._items.append(self._active_item)
            self._active_item = None
            self.update()

    def _erase_near(self, img_pt: QPointF) -> None:
        threshold = 14.0
        for idx in range(len(self._items) - 1, -1, -1):
            item = self._items[idx]
            if hit_test_annotation_points(
                img_pt.x(), img_pt.y(), item.kind, item.points, threshold=threshold
            ):
                self._push_undo()
                self._items.pop(idx)
                self.update()
                return

    def _draw_annotation_item(
        self, painter: QPainter, item: AnnotationItem, backing: QPixmap
    ) -> None:
        if not item.points:
            return

        painter.save()
        color = QColor(item.color)
        if item.kind == "highlighter":
            color.setAlpha(105)
            pen = QPen(
                color,
                item.width,
                Qt.PenStyle.SolidLine,
                Qt.PenCapStyle.FlatCap,
                Qt.PenJoinStyle.RoundJoin,
            )
            painter.setPen(pen)
            path = QPainterPath(QPointF(*item.points[0]))
            for px, py in item.points[1:]:
                path.lineTo(px, py)
            painter.drawPath(path)

        elif item.kind == "pen":
            pen = QPen(
                color,
                item.width,
                Qt.PenStyle.SolidLine,
                Qt.PenCapStyle.RoundCap,
                Qt.PenJoinStyle.RoundJoin,
            )
            painter.setPen(pen)
            if len(item.points) == 1:
                painter.drawPoint(QPointF(*item.points[0]))
            else:
                path = QPainterPath(QPointF(*item.points[0]))
                for px, py in item.points[1:]:
                    path.lineTo(px, py)
                painter.drawPath(path)

        elif len(item.points) >= 2:
            p1 = QPointF(*item.points[0])
            p2 = QPointF(*item.points[1])
            rect = QRectF(p1, p2).normalized()
            pen = QPen(color, item.width, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap)
            painter.setPen(pen)

            if item.kind == "rect":
                painter.drawRect(rect)
            elif item.kind == "ellipse":
                painter.drawEllipse(rect)
            elif item.kind == "line":
                painter.drawLine(p1, p2)
            elif item.kind == "arrow":
                painter.drawLine(p1, p2)
                left_pt, right_pt = compute_arrow_wings(
                    p1.x(), p1.y(), p2.x(), p2.y(), item.width
                )
                painter.drawLine(p2, QPointF(*left_pt))
                painter.drawLine(p2, QPointF(*right_pt))
            elif item.kind == "redact":
                painter.fillRect(rect, QColor("#0f172a"))
            elif item.kind == "pixelate":
                aligned = rect.toAlignedRect().intersected(backing.rect())
                if aligned.width() >= 4 and aligned.height() >= 4:
                    sub = backing.copy(aligned).toImage()
                    small_w = max(2, aligned.width() // 10)
                    small_h = max(2, aligned.height() // 10)
                    mosaic = sub.scaled(
                        small_w,
                        small_h,
                        Qt.AspectRatioMode.IgnoreAspectRatio,
                        Qt.TransformationMode.FastTransformation,
                    ).scaled(
                        aligned.width(),
                        aligned.height(),
                        Qt.AspectRatioMode.IgnoreAspectRatio,
                        Qt.TransformationMode.FastTransformation,
                    )
                    painter.drawImage(aligned.topLeft(), QImage(mosaic))

        painter.restore()

    def paintEvent(self, _event: object) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        painter.fillRect(self.rect(), QColor("#f1f5f9"))

        if self._pixmap.isNull():
            painter.setPen(QColor("#64748b"))
            painter.setFont(QFont("Sans Serif", 11))
            painter.drawText(
                self.rect(),
                Qt.AlignmentFlag.AlignCenter,
                "Click '+ New' or press Ctrl+N to capture a snip.",
            )
            painter.end()
            return

        rendered = self.render_final_pixmap()
        if self._active_item is not None:
            temp_painter = QPainter(rendered)
            temp_painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
            self._draw_annotation_item(temp_painter, self._active_item, rendered)
            temp_painter.end()

        origin = self._canvas_origin()
        painter.save()
        painter.translate(origin)
        painter.scale(self._zoom, self._zoom)

        # Subtle crisp border around canvas
        painter.setPen(QPen(QColor("#cbd5e1"), 1.0 / self._zoom))
        painter.drawRect(QRectF(-1, -1, rendered.width() + 2, rendered.height() + 2))
        painter.drawPixmap(0, 0, rendered)

        if self._ruler_visible:
            self._paint_ruler(painter)

        if self._crop_active:
            dim = QPainterPath()
            dim.addRect(0, 0, rendered.width(), rendered.height())
            cut = QPainterPath()
            cut.addRect(self._crop_rect)
            painter.fillPath(dim.subtracted(cut), QColor(15, 23, 42, 120))
            painter.setPen(QPen(QColor("#2563eb"), 2.0 / self._zoom, Qt.PenStyle.DashLine))
            painter.drawRect(self._crop_rect)

        painter.restore()
        painter.end()

    def _paint_ruler(self, painter: QPainter) -> None:
        painter.save()
        painter.translate(self._ruler_center)
        painter.rotate(self._ruler_angle_deg)
        ruler_rect = QRectF(-210.0, -28.0, 420.0, 56.0)
        painter.fillRect(ruler_rect, QColor(248, 250, 252, 225))
        painter.setPen(QPen(QColor("#475569"), 1.5))
        painter.drawRect(ruler_rect)

        painter.setPen(QPen(QColor("#64748b"), 1.0))
        for x in range(-200, 205, 10):
            tick_h = 12.0 if x % 50 == 0 else 6.0
            painter.drawLine(QPointF(float(x), -28.0), QPointF(float(x), -28.0 + tick_h))
            painter.drawLine(QPointF(float(x), 28.0), QPointF(float(x), 28.0 - tick_h))

        painter.setFont(QFont("Monospace", 9, QFont.Weight.DemiBold))
        painter.setPen(QColor("#0f172a"))
        angle_label = f"{int(round(self._ruler_angle_deg)) % 180}°"
        painter.drawText(ruler_rect, Qt.AlignmentFlag.AlignCenter, f"Ruler · {angle_label}")
        painter.restore()
