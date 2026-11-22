"""Fullscreen Windows 11-style Snipping Overlay for Rectangle, Window, Fullscreen, and Freeform snips."""

from __future__ import annotations

from typing import List, Optional

from PyQt6.QtCore import QPoint, QPointF, QRect, Qt, pyqtSignal
from PyQt6.QtGui import (
    QColor,
    QCursor,
    QFont,
    QGuiApplication,
    QKeyEvent,
    QMouseEvent,
    QPainter,
    QPainterPath,
    QPen,
    QPixmap,
    QPolygonF,
)
from PyQt6.QtWidgets import QFrame, QHBoxLayout, QPushButton, QWidget

from .display_backend import WindowGeometry


class CaptureOverlay(QWidget):
    """Interactive dim-scrim overlay replicating Windows 11 Snipping Tool's capture bar and modes."""

    snip_completed = pyqtSignal(QPixmap, str)
    snip_cancelled = pyqtSignal()

    def __init__(
        self,
        desktop_pixmap: QPixmap,
        origin_x: int = 0,
        origin_y: int = 0,
        initial_mode: str = "rectangle",
        windows: Optional[List[WindowGeometry]] = None,
    ) -> None:
        super().__init__(None)
        self._desktop_pixmap = desktop_pixmap
        self._origin_x = origin_x
        self._origin_y = origin_y
        self._mode = initial_mode
        self._windows = windows or []

        self._dragging = False
        self._start_pos = QPoint()
        self._current_pos = QPoint()
        self._lasso_points: List[QPointF] = []
        self._hovered_window: Optional[WindowGeometry] = None

        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, False)
        self.setMouseTracking(True)
        self.setCursor(QCursor(Qt.CursorShape.CrossCursor))
        self.setGeometry(origin_x, origin_y, desktop_pixmap.width(), desktop_pixmap.height())

        self._build_floating_mode_bar()

        if self._mode == "fullscreen":
            # Fullscreen snip completes immediately once shown
            self._complete_fullscreen()

    def _build_floating_mode_bar(self) -> None:
        self._bar = QFrame(self)
        self._bar.setStyleSheet(
            """
            QFrame {
                background-color: #1e293b;
                border: 1px solid #334155;
                border-radius: 8px;
            }
            QPushButton {
                color: #e2e8f0;
                background: transparent;
                border: none;
                border-radius: 6px;
                padding: 6px 12px;
                font-size: 12px;
                font-weight: 600;
            }
            QPushButton:hover {
                background-color: #334155;
            }
            QPushButton[active="true"] {
                background-color: #2563eb;
                color: #ffffff;
            }
            """
        )
        layout = QHBoxLayout(self._bar)
        layout.setContentsMargins(6, 4, 6, 4)
        layout.setSpacing(4)

        self._mode_buttons = {}
        modes = [
            ("rectangle", "Rectangle"),
            ("window", "Window"),
            ("fullscreen", "Fullscreen"),
            ("freeform", "Freeform"),
        ]
        for key, label in modes:
            btn = QPushButton(label, self._bar)
            btn.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
            btn.setProperty("active", "true" if key == self._mode else "false")
            btn.clicked.connect(lambda _checked=False, m=key: self._switch_mode(m))
            layout.addWidget(btn)
            self._mode_buttons[key] = btn

        cancel_btn = QPushButton("Cancel (Esc)", self._bar)
        cancel_btn.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        cancel_btn.clicked.connect(self._cancel)
        layout.addWidget(cancel_btn)

        self._bar.adjustSize()
        primary = QGuiApplication.primaryScreen()
        if primary is not None:
            pgeom = primary.geometry()
            bx = (pgeom.x() - self._origin_x) + (pgeom.width() - self._bar.width()) // 2
            by = (pgeom.y() - self._origin_y) + 20
        else:
            bx = (self.width() - self._bar.width()) // 2
            by = 20
        self._bar.move(max(12, bx), max(12, by))

    def _switch_mode(self, mode: str) -> None:
        self._mode = mode
        self._dragging = False
        self._lasso_points.clear()
        for key, btn in self._mode_buttons.items():
            btn.setProperty("active", "true" if key == mode else "false")
            btn.style().unpolish(btn)
            btn.style().polish(btn)

        if mode == "fullscreen":
            self._complete_fullscreen()
        else:
            self.update()

    def _complete_fullscreen(self) -> None:
        self._bar.hide()
        self.hide()
        self.snip_completed.emit(self._desktop_pixmap.copy(), "fullscreen")
        self.close()

    def _cancel(self) -> None:
        self.hide()
        self.snip_cancelled.emit()
        self.close()

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Escape:
            self._cancel()
            return
        super().keyPressEvent(event)

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.RightButton:
            self._cancel()
            return
        if event.button() != Qt.MouseButton.LeftButton:
            return

        pos = event.position().toPoint()
        if self._mode == "window":
            if self._hovered_window is not None:
                rect = self._window_to_local_rect(self._hovered_window)
                cropped = self._desktop_pixmap.copy(rect)
                self.hide()
                self.snip_completed.emit(cropped, "window")
                self.close()
            else:
                # Fallback if no individual window metadata available: capture hovered screen
                self._complete_fullscreen()
            return

        self._dragging = True
        self._start_pos = pos
        self._current_pos = pos
        if self._mode == "freeform":
            self._lasso_points = [QPointF(pos)]
        self.update()

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        pos = event.position().toPoint()
        self._current_pos = pos

        if self._mode == "window":
            gx = pos.x() + self._origin_x
            gy = pos.y() + self._origin_y
            hovered = None
            for win in self._windows:
                if win.x <= gx <= win.x + win.width and win.y <= gy <= win.y + win.height:
                    hovered = win
                    break
            if hovered != self._hovered_window:
                self._hovered_window = hovered
                self.update()
            return

        if self._dragging:
            if self._mode == "freeform":
                self._lasso_points.append(QPointF(pos))
            self.update()

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if event.button() != Qt.MouseButton.LeftButton or not self._dragging:
            return
        self._dragging = False
        self._current_pos = event.position().toPoint()

        if self._mode == "rectangle":
            rect = QRect(self._start_pos, self._current_pos).normalized()
            rect = rect.intersected(self.rect())
            if rect.width() >= 6 and rect.height() >= 6:
                cropped = self._desktop_pixmap.copy(rect)
                self.hide()
                self.snip_completed.emit(cropped, "rectangle")
                self.close()
            else:
                self.update()
            return

        if self._mode == "freeform" and len(self._lasso_points) >= 3:
            polygon = QPolygonF(self._lasso_points)
            bounds = polygon.boundingRect().toAlignedRect().intersected(self.rect())
            if bounds.width() >= 8 and bounds.height() >= 8:
                result = QPixmap(bounds.size())
                result.fill(Qt.GlobalColor.transparent)
                painter = QPainter(result)
                painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
                clip_path = QPainterPath()
                translated_poly = polygon.translated(-bounds.x(), -bounds.y())
                clip_path.addPolygon(translated_poly)
                clip_path.closeSubpath()
                painter.setClipPath(clip_path)
                painter.drawPixmap(0, 0, self._desktop_pixmap, bounds.x(), bounds.y(), bounds.width(), bounds.height())
                painter.end()
                self.hide()
                self.snip_completed.emit(result, "freeform")
                self.close()
            else:
                self._lasso_points.clear()
                self.update()

    def _window_to_local_rect(self, win: WindowGeometry) -> QRect:
        rect = QRect(
            win.x - self._origin_x,
            win.y - self._origin_y,
            win.width,
            win.height,
        )
        return rect.intersected(self.rect())

    def paintEvent(self, _event: object) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        painter.drawPixmap(0, 0, self._desktop_pixmap)

        dim_path = QPainterPath()
        dim_path.addRect(0, 0, self.width(), self.height())

        highlight_rect: Optional[QRect] = None
        label_text = ""

        if self._mode == "rectangle" and self._dragging:
            highlight_rect = QRect(self._start_pos, self._current_pos).normalized()
            cutout = QPainterPath()
            cutout.addRect(
                float(highlight_rect.x()),
                float(highlight_rect.y()),
                float(highlight_rect.width()),
                float(highlight_rect.height()),
            )
            dim_path = dim_path.subtracted(cutout)
            label_text = f"{highlight_rect.width()} × {highlight_rect.height()} px"

        elif self._mode == "window" and self._hovered_window is not None:
            highlight_rect = self._window_to_local_rect(self._hovered_window)
            cutout = QPainterPath()
            cutout.addRect(
                float(highlight_rect.x()),
                float(highlight_rect.y()),
                float(highlight_rect.width()),
                float(highlight_rect.height()),
            )
            dim_path = dim_path.subtracted(cutout)
            title = self._hovered_window.title[:42]
            label_text = f"{title} ({highlight_rect.width()} × {highlight_rect.height()} px)"

        elif self._mode == "freeform" and len(self._lasso_points) >= 2:
            poly = QPolygonF(self._lasso_points)
            cutout = QPainterPath()
            cutout.addPolygon(poly)
            cutout.closeSubpath()
            dim_path = dim_path.subtracted(cutout)

        painter.fillPath(dim_path, QColor(15, 23, 42, 120))

        pen = QPen(QColor("#38bdf8"), 2.0)
        painter.setPen(pen)

        if highlight_rect is not None:
            painter.drawRect(highlight_rect)
            if label_text:
                font = QFont("Sans Serif", 9, QFont.Weight.DemiBold)
                painter.setFont(font)
                metrics = painter.fontMetrics()
                tw = metrics.horizontalAdvance(label_text) + 16
                th = metrics.height() + 8
                lx = highlight_rect.x()
                ly = max(8, highlight_rect.y() - th - 6)
                badge = QRect(lx, ly, tw, th)
                painter.fillRect(badge, QColor(15, 23, 42, 225))
                painter.setPen(QColor("#f8fafc"))
                painter.drawText(badge, Qt.AlignmentFlag.AlignCenter, label_text)

        elif self._mode == "freeform" and len(self._lasso_points) >= 2:
            painter.drawPolyline(QPolygonF(self._lasso_points))

        painter.end()
