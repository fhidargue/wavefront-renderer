import time
from pathlib import Path

from PySide6.QtWidgets import (
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QPushButton,
    QStatusBar,
    QLabel,
    QProgressBar,
)
from PySide6.QtCore import QTimer, Qt

from gui.render.display import RenderDisplay
from gui.render.worker import RenderWorker


class RenderWindow(QMainWindow):
    def __init__(
        self,
        renderer_path: str,
        scene_path: str,
        output_path: str,
        width: int = 600,
        height: int = 600,
        parent=None,
    ):
        super().__init__(parent)
        self.renderer_path = renderer_path
        self.scene_path = scene_path
        self.output_path = output_path
        self.width = width
        self.height = height
        self.worker = None
        self.start_time = None

        self.setWindowTitle(f"Wavefront Renderer - {Path(scene_path).name}")

        self._build_ui()
        self._build_poll_timer()

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)

        # Render display
        self.display = RenderDisplay(self.width, self.height)
        layout.addWidget(self.display)

        # Progress bar
        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        self.progress_bar.setFixedHeight(6)
        self.progress_bar.setTextVisible(False)
        layout.addWidget(self.progress_bar)

        # Controls row
        controls = QHBoxLayout()
        controls.setSpacing(6)

        self.render_btn = QPushButton("Render")
        self.render_btn.setFixedHeight(32)
        self.render_btn.clicked.connect(self.start_render)

        self.stop_btn = QPushButton("Stop")
        self.stop_btn.setFixedHeight(32)
        self.stop_btn.setEnabled(False)
        self.stop_btn.clicked.connect(self.stop_render)

        self.scene_label = QLabel(Path(self.scene_path).name)
        self.scene_label.setStyleSheet("color: grey; font-size: 12px;")

        controls.addWidget(self.render_btn)
        controls.addWidget(self.stop_btn)
        controls.addStretch()
        controls.addWidget(self.scene_label)
        layout.addLayout(controls)

        # Status bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Press Render to start")

        self.adjustSize()

    def _build_poll_timer(self):
        """
        Refresh the output file while rendering
        """
        self.poll_timer = QTimer()
        self.poll_timer.setInterval(500)
        self.poll_timer.timeout.connect(self._poll_output)

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            self.close()
        else:
            super().keyPressEvent(event)

    def start_render(self):
        if self.worker and self.worker.isRunning():
            return

        self.start_time = time.time()
        self.progress_bar.setValue(0)
        self.render_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)
        self.status_bar.showMessage("Rendering...")

        self.worker = RenderWorker(
            renderer_path=self.renderer_path,
            scene_path=self.scene_path,
            output_path=self.output_path,
        )
        self.worker.statusUpdate.connect(self._on_status_update)
        self.worker.progressUpdate.connect(self._on_progress_update)
        self.worker.renderComplete.connect(self._on_render_complete)
        self.worker.renderFailed.connect(self._on_render_failed)
        self.worker.start()

        self.poll_timer.start()

    def stop_render(self):
        if self.worker:
            self.worker.stop()

        self.poll_timer.stop()
        self._set_idle("Stopped")

    def _on_status_update(self, line: str):
        if line.strip():
            self.status_bar.showMessage(line.strip())

    def _on_render_complete(self, elapsed_ms: float, output_path: str):
        self.poll_timer.stop()
        # Show final output (not preview) on completion
        self.display.update_from_file(output_path)
        self.progress_bar.setValue(100)
        self._set_idle(f"Done. {elapsed_ms / 1000:.1f}s | {output_path}")

    def _on_render_failed(self, error: str):
        self.poll_timer.stop()
        self._set_idle(f"Error: {error}")
        print(f"Render failed: {error}")

    def _on_progress_update(self, current: int, total: float):
        percent = int((current / total) * 100)
        self.progress_bar.setValue(percent)
        self.status_bar.showMessage(f"Sample {current}/{int(total)}")

    def _poll_output(self):
        """
        Poll the preview file during rendering
        """
        if self.worker:
            preview = Path(self.worker.preview_path)

            if preview.exists() and preview.stat().st_size > 0:
                elapsed = time.time() - self.start_time
                self.status_bar.showMessage(f"Rendering... {elapsed:.1f}s")
                self.display.update_from_file(str(preview))

    def _set_idle(self, message: str):
        self.render_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self.status_bar.showMessage(message)
