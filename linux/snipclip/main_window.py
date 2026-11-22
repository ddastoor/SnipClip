"""Windows 11 Snipping Tool Main Window for Debian Linux (X11 & Wayland)."""

from __future__ import annotations

from pathlib import Path
from typing import Optional

from PyQt6.QtCore import QBuffer, QByteArray, QIODevice, QTimer, Qt
from PyQt6.QtGui import QAction, QColor, QGuiApplication, QKeySequence, QPixmap
from PyQt6.QtWidgets import (
    QColorDialog,
    QComboBox,
    QFileDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QScrollArea,
    QSlider,
    QStatusBar,
    QVBoxLayout,
    QWidget,
)

from .canvas_editor import SnipCanvasEditor
from .capture_overlay import CaptureOverlay
from .display_backend import (
    DisplaySessionInfo,
    capture_wayland_screenshot_to_file,
    copy_png_bytes_to_system_clipboard,
    detect_display_session,
    enumerate_x11_windows,
    grab_full_virtual_desktop_qt,
)
from .version import DEB_FILENAME, GITHUB_RELEASE_TAG, __version__


class SnipClipMainWindow(QMainWindow):
    """Main desktop application window modeled closely after the Windows 11 Snipping Tool."""

    def __init__(
        self,
        session: Optional[DisplaySessionInfo] = None,
        initial_mode: str = "rectangle",
        initial_delay: int = 0,
        autostart_snip: bool = False,
    ) -> None:
        super().__init__()
        self._session = session or detect_display_session()
        self._snip_mode = initial_mode
        self._delay_seconds = initial_delay
        self._overlay: Optional[CaptureOverlay] = None
        self._countdown_remaining = 0

        self.setWindowTitle(f"SnipClip v{__version__}")
        self.resize(1040, 700)
        self._apply_theme()

        central = QWidget(self)
        root_layout = QVBoxLayout(central)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)

        self._editor = SnipCanvasEditor(self)
        self._build_command_bar(root_layout)
        self._build_crop_banner(root_layout)

        scroll = QScrollArea(self)
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setWidget(self._editor)
        root_layout.addWidget(scroll, stretch=1)

        self.setCentralWidget(central)
        self._build_status_bar()
        self._bind_shortcuts()

        self._editor.history_changed.connect(self._on_history_changed)
        self._editor.dimensions_changed.connect(self._on_dimensions_changed)
        self._editor.zoom_changed.connect(self._on_zoom_changed)
        self._editor.crop_mode_changed.connect(self._on_crop_mode_changed)

        if autostart_snip:
            QTimer.singleShot(200, self.trigger_new_snip)

    def _apply_theme(self) -> None:
        self.setStyleSheet(
            """
            QMainWindow {
                background-color: #f8fafc;
            }
            QFrame#CommandBar {
                background-color: #ffffff;
                border-bottom: 1px solid #e2e8f0;
            }
            QFrame#CropBanner {
                background-color: #eff6ff;
                border-bottom: 1px solid #bfdbfe;
            }
            QPushButton {
                background-color: #ffffff;
                color: #0f172a;
                border: 1px solid #cbd5e1;
                border-radius: 6px;
                padding: 6px 11px;
                font-size: 12px;
                font-weight: 600;
            }
            QPushButton:hover {
                background-color: #f1f5f9;
            }
            QPushButton:disabled {
                color: #94a3b8;
                border-color: #e2e8f0;
            }
            QPushButton#PrimaryButton {
                background-color: #0f172a;
                color: #ffffff;
                border-color: #0f172a;
            }
            QPushButton#PrimaryButton:hover {
                background-color: #1e293b;
            }
            QPushButton[activeTool="true"] {
                background-color: #dbeafe;
                border-color: #2563eb;
                color: #1d4ed8;
            }
            QComboBox {
                background-color: #ffffff;
                color: #0f172a;
                border: 1px solid #cbd5e1;
                border-radius: 6px;
                padding: 5px 10px;
                font-size: 12px;
            }
            """
        )

    def _build_command_bar(self, parent_layout: QVBoxLayout) -> None:
        bar = QFrame(self)
        bar.setObjectName("CommandBar")
        row = QHBoxLayout(bar)
        row.setContentsMargins(12, 8, 12, 8)
        row.setSpacing(6)

        self._new_btn = QPushButton("+ New", bar)
        self._new_btn.setObjectName("PrimaryButton")
        self._new_btn.setToolTip("Start a new snip (Ctrl+N)")
        self._new_btn.clicked.connect(self.trigger_new_snip)
        row.addWidget(self._new_btn)

        self._mode_combo = QComboBox(bar)
        self._mode_combo.addItem("Rectangle", "rectangle")
        self._mode_combo.addItem("Window", "window")
        self._mode_combo.addItem("Fullscreen", "fullscreen")
        self._mode_combo.addItem("Freeform", "freeform")
        idx = self._mode_combo.findData(self._snip_mode)
        if idx >= 0:
            self._mode_combo.setCurrentIndex(idx)
        self._mode_combo.currentIndexChanged.connect(self._on_mode_selected)
        row.addWidget(self._mode_combo)

        self._delay_combo = QComboBox(bar)
        self._delay_combo.addItem("No delay", 0)
        self._delay_combo.addItem("3s delay", 3)
        self._delay_combo.addItem("5s delay", 5)
        self._delay_combo.addItem("10s delay", 10)
        d_idx = self._delay_combo.findData(self._delay_seconds)
        if d_idx >= 0:
            self._delay_combo.setCurrentIndex(d_idx)
        self._delay_combo.currentIndexChanged.connect(self._on_delay_selected)
        row.addWidget(self._delay_combo)

        row.addSpacing(8)

        self._tool_buttons = {}
        tools = [
            ("pen", "Pen"),
            ("highlighter", "Highlighter"),
            ("eraser", "Eraser"),
            ("rect", "Rectangle"),
            ("ellipse", "Ellipse"),
            ("arrow", "Arrow"),
            ("redact", "Redact"),
            ("pixelate", "Blur"),
            ("crop", "Crop"),
        ]
        for key, label in tools:
            btn = QPushButton(label, bar)
            btn.setProperty("activeTool", "true" if key == "pen" else "false")
            btn.clicked.connect(lambda _c=False, t=key: self._select_tool(t))
            row.addWidget(btn)
            self._tool_buttons[key] = btn

        self._ruler_btn = QPushButton("Ruler", bar)
        self._ruler_btn.setCheckable(True)
        self._ruler_btn.toggled.connect(self._editor.toggle_ruler)
        row.addWidget(self._ruler_btn)

        self._color_btn = QPushButton("Color", bar)
        self._color_btn.clicked.connect(self._choose_color)
        row.addWidget(self._color_btn)

        self._width_slider = QSlider(Qt.Orientation.Horizontal, bar)
        self._width_slider.setRange(1, 16)
        self._width_slider.setValue(3)
        self._width_slider.setFixedWidth(72)
        self._width_slider.setToolTip("Stroke thickness")
        self._width_slider.valueChanged.connect(lambda v: self._editor.set_stroke_width(float(v)))
        row.addWidget(self._width_slider)

        row.addStretch(1)

        self._undo_btn = QPushButton("Undo", bar)
        self._undo_btn.setEnabled(False)
        self._undo_btn.clicked.connect(self._editor.undo)
        row.addWidget(self._undo_btn)

        self._redo_btn = QPushButton("Redo", bar)
        self._redo_btn.setEnabled(False)
        self._redo_btn.clicked.connect(self._editor.redo)
        row.addWidget(self._redo_btn)

        copy_btn = QPushButton("Copy", bar)
        copy_btn.setToolTip("Copy snip to clipboard (Ctrl+C)")
        copy_btn.clicked.connect(self.copy_to_clipboard)
        row.addWidget(copy_btn)

        save_btn = QPushButton("Save", bar)
        save_btn.setToolTip("Save snip to disk (Ctrl+S)")
        save_btn.clicked.connect(self.save_as_file)
        row.addWidget(save_btn)

        open_btn = QPushButton("Open", bar)
        open_btn.setToolTip("Open an image file (Ctrl+O)")
        open_btn.clicked.connect(self.open_image_file)
        row.addWidget(open_btn)

        about_btn = QPushButton(f"v{__version__}", bar)
        about_btn.setToolTip("About SnipClip & Display Server Details")
        about_btn.clicked.connect(self.show_about_dialog)
        row.addWidget(about_btn)

        parent_layout.addWidget(bar)

    def _build_crop_banner(self, parent_layout: QVBoxLayout) -> None:
        self._crop_banner = QFrame(self)
        self._crop_banner.setObjectName("CropBanner")
        layout = QHBoxLayout(self._crop_banner)
        layout.setContentsMargins(14, 6, 14, 6)
        layout.addWidget(QLabel("Drag on the image to adjust the crop region.", self._crop_banner))
        layout.addStretch(1)

        apply_btn = QPushButton("Apply Crop", self._crop_banner)
        apply_btn.setObjectName("PrimaryButton")
        apply_btn.clicked.connect(self._editor.apply_crop)
        layout.addWidget(apply_btn)

        cancel_btn = QPushButton("Cancel", self._crop_banner)
        cancel_btn.clicked.connect(self._editor.cancel_crop)
        layout.addWidget(cancel_btn)

        self._crop_banner.hide()
        parent_layout.addWidget(self._crop_banner)

    def _build_status_bar(self) -> None:
        status = QStatusBar(self)
        self.setStatusBar(status)
        protocol_upper = self._session.protocol.upper()
        self._session_label = QLabel(
            f"Auto-detected: {protocol_upper} ({self._session.detection_source}) · "
            f"{self._session.desktop_environment} · {self._session.capture_driver}"
        )
        self._dims_label = QLabel("No snip loaded")
        self._zoom_label = QLabel("100%")
        status.addWidget(self._session_label, stretch=1)
        status.addPermanentWidget(self._dims_label)
        status.addPermanentWidget(self._zoom_label)

    def _bind_shortcuts(self) -> None:
        shortcuts = [
            ("Ctrl+N", self.trigger_new_snip),
            ("Ctrl+C", self.copy_to_clipboard),
            ("Ctrl+S", self.save_as_file),
            ("Ctrl+O", self.open_image_file),
            ("Ctrl+Z", self._editor.undo),
            ("Ctrl+Y", self._editor.redo),
            ("Ctrl++", lambda: self._editor.set_zoom(self._editor.zoom_factor() + 0.25)),
            ("Ctrl+-", lambda: self._editor.set_zoom(self._editor.zoom_factor() - 0.25)),
            ("Ctrl+0", lambda: self._editor.set_zoom(1.0)),
        ]
        for seq, handler in shortcuts:
            action = QAction(self)
            action.setShortcut(QKeySequence(seq))
            action.triggered.connect(handler)
            self.addAction(action)

    def _select_tool(self, tool: str) -> None:
        self._editor.set_tool(tool)
        for key, btn in self._tool_buttons.items():
            btn.setProperty("activeTool", "true" if key == tool else "false")
            btn.style().unpolish(btn)
            btn.style().polish(btn)

    def _choose_color(self) -> None:
        chosen = QColorDialog.getColor(QColor("#ef4444"), self, "Select Ink / Shape Color")
        if chosen.isValid():
            self._editor.set_color(chosen.name())

    def _on_mode_selected(self) -> None:
        self._snip_mode = str(self._mode_combo.currentData())

    def _on_delay_selected(self) -> None:
        self._delay_seconds = int(self._delay_combo.currentData())

    def _on_history_changed(self, can_undo: bool, can_redo: bool) -> None:
        self._undo_btn.setEnabled(can_undo)
        self._redo_btn.setEnabled(can_redo)

    def _on_dimensions_changed(self, width: int, height: int) -> None:
        self._dims_label.setText(f"{width} × {height} px")

    def _on_zoom_changed(self, zoom: float) -> None:
        self._zoom_label.setText(f"{int(round(zoom * 100))}%")

    def _on_crop_mode_changed(self, active: bool) -> None:
        self._crop_banner.setVisible(active)

    def trigger_new_snip(self) -> None:
        if self._delay_seconds > 0:
            self._countdown_remaining = self._delay_seconds
            self._new_btn.setEnabled(False)
            self._tick_countdown()
        else:
            self._begin_capture_now()

    def _tick_countdown(self) -> None:
        if self._countdown_remaining <= 0:
            self._new_btn.setText("+ New")
            self._new_btn.setEnabled(True)
            self._begin_capture_now()
            return
        self._new_btn.setText(f"Snipping in {self._countdown_remaining}s…")
        self._countdown_remaining -= 1
        QTimer.singleShot(1000, self._tick_countdown)

    def _begin_capture_now(self) -> None:
        self.hide()
        QGuiApplication.processEvents()
        QTimer.singleShot(180, self._capture_desktop_and_launch_overlay)

    def _capture_desktop_and_launch_overlay(self) -> None:
        desktop_pixmap: Optional[QPixmap] = None
        origin_x = 0
        origin_y = 0

        if self._session.protocol == "wayland":
            # On Wayland Window mode, XDG Portal interactive mode provides native compositor window picking
            if self._snip_mode == "window":
                interactive_file = capture_wayland_screenshot_to_file(interactive=True)
                if interactive_file is not None:
                    pix = QPixmap(str(interactive_file))
                    interactive_file.unlink(missing_ok=True)
                    if not pix.isNull():
                        self._on_snip_completed(pix, "window")
                        return

            shot_path = capture_wayland_screenshot_to_file(interactive=False)
            if shot_path is not None:
                loaded = QPixmap(str(shot_path))
                shot_path.unlink(missing_ok=True)
                if not loaded.isNull():
                    desktop_pixmap = loaded

        if desktop_pixmap is None:
            grabbed, origin_x, origin_y = grab_full_virtual_desktop_qt()
            if isinstance(grabbed, QPixmap) and not grabbed.isNull():
                desktop_pixmap = grabbed

        if desktop_pixmap is None or desktop_pixmap.isNull():
            self.show()
            QMessageBox.warning(
                self,
                "Capture Unavailable",
                "Unable to capture desktop pixels. On Wayland, ensure xdg-desktop-portal or grim is installed.",
            )
            return

        windows = enumerate_x11_windows() if self._session.protocol == "x11" else []
        self._overlay = CaptureOverlay(
            desktop_pixmap=desktop_pixmap,
            origin_x=origin_x,
            origin_y=origin_y,
            initial_mode=self._snip_mode,
            windows=windows,
        )
        self._overlay.snip_completed.connect(self._on_snip_completed)
        self._overlay.snip_cancelled.connect(self._on_snip_cancelled)
        if self._snip_mode != "fullscreen":
            self._overlay.show()

    def _on_snip_completed(self, pixmap: QPixmap, _mode: str) -> None:
        self._editor.set_pixmap(pixmap)
        self.copy_to_clipboard(silent=True)
        self.show()
        self.raise_()
        self.activateWindow()
        self.statusBar().showMessage("Snip captured and copied to clipboard.", 4000)

    def _on_snip_cancelled(self) -> None:
        self.show()
        self.raise_()
        self.activateWindow()

    def copy_to_clipboard(self, silent: bool = False) -> None:
        if not self._editor.has_image():
            return
        final_pix = self._editor.render_final_pixmap()
        QGuiApplication.clipboard().setPixmap(final_pix)

        buf = QByteArray()
        qbuf = QBuffer(buf)
        qbuf.open(QIODevice.OpenModeFlag.WriteOnly)
        final_pix.save(qbuf, "PNG")
        qbuf.close()
        copy_png_bytes_to_system_clipboard(bytes(buf), self._session)

        if not silent:
            self.statusBar().showMessage("Copied annotated snip to clipboard.", 3000)

    def save_as_file(self) -> None:
        if not self._editor.has_image():
            return
        default_dir = str(Path.home() / "Pictures" / "SnipClip-Capture.png")
        target, _ = QFileDialog.getSaveFileName(
            self,
            "Save Snip As",
            default_dir,
            "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;WebP Image (*.webp)",
        )
        if not target:
            return
        final_pix = self._editor.render_final_pixmap()
        if final_pix.save(target):
            self.statusBar().showMessage(f"Saved snip to {target}", 4000)

    def open_image_file(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Open Image in SnipClip",
            str(Path.home()),
            "Images (*.png *.jpg *.jpeg *.webp *.bmp)",
        )
        if not path:
            return
        loaded = QPixmap(path)
        if not loaded.isNull():
            self._editor.set_pixmap(loaded)

    def show_about_dialog(self) -> None:
        QMessageBox.information(
            self,
            f"About SnipClip v{__version__}",
            (
                f"<b>SnipClip v{__version__}</b><br>"
                f"Windows Snipping Tool clone for Debian-based Linux.<br><br>"
                f"<b>GitHub Release Tag:</b> {GITHUB_RELEASE_TAG}<br>"
                f"<b>Debian Package:</b> {DEB_FILENAME}<br><br>"
                f"<b>Detected Display Server:</b> {self._session.protocol.upper()}<br>"
                f"<b>Desktop Environment:</b> {self._session.desktop_environment}<br>"
                f"<b>Capture Backend:</b> {self._session.capture_driver}<br>"
                f"<b>Clipboard Backend:</b> {self._session.clipboard_driver}"
            ),
        )
