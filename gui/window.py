import time
from pathlib import Path

from PySide6.QtWidgets import (
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QPushButton,
    QLabel,
    QProgressBar,
    QSizePolicy,
)
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QFontMetrics

from gui.render.display import RenderDisplay
from gui.render.worker import RenderWorker


class RenderWindow(QMainWindow):
    def __init__(
        self,
        renderer_path: str,
        scene_path: str,
        output_path: str,
        camera_path: str,
        width: int = 600,
        height: int = 600,
        denoise: bool = False,
        env_path: str = "",
        cost_rr: bool = True,
        ray_sort: bool = True,
        samples: int | None = None,
        adaptive_sampling: bool = True,
        parent=None,
    ):
        super().__init__(parent)
        self.renderer_path = renderer_path
        self.scene_path = scene_path
        self.output_path = output_path
        self.camera_path = camera_path
        self.width = width
        self.height = height
        self.denoise = denoise
        self.env_path = env_path
        self.cost_rr = cost_rr
        self.ray_sort = ray_sort
        self.samples = samples
        self.adaptive_sampling = adaptive_sampling
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

        # Status row
        status_row = QHBoxLayout()
        status_row.setSpacing(6)

        self.status_label = QLabel("Press Render to start")
        self.status_label.setStyleSheet("color: grey; font-size: 12px;")
        self.status_label.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred
        )
        self.status_label.setMinimumWidth(0)

        self.progress_label = QLabel("")
        self.progress_label.setStyleSheet("color: grey; font-size: 12px;")
        self.progress_label.setAlignment(Qt.AlignmentFlag.AlignRight)

        status_row.addWidget(self.status_label, 1)
        status_row.addWidget(self.progress_label)
        layout.addLayout(status_row)

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
        self.display.clear()
        self.render_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)
        self._set_status_text("Rendering...")
        self.progress_label.setText("")

        self.worker = RenderWorker(
            renderer_path=self.renderer_path,
            scene_path=self.scene_path,
            output_path=self.output_path,
            camera_path=self.camera_path,
            width=self.width,
            height=self.height,
            denoise=self.denoise,
            env_path=self.env_path,
            cost_rr=self.cost_rr,
            ray_sort=self.ray_sort,
            samples=self.samples,
            adaptive_sampling=self.adaptive_sampling,
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

    def _set_status_text(self, text: str):
        """
        Elides long status text with '...' instead of letting it force
        the window wider or get silently clipped
        """
        metrics = QFontMetrics(self.status_label.font())
        elided = metrics.elidedText(
            text, Qt.TextElideMode.ElideMiddle, self.status_label.width()
        )
        self.status_label.setText(elided)

    def _on_status_update(self, line: str):
        if line.strip():
            self._set_status_text(line.strip())

    def _on_render_complete(self, elapsed_ms: float, output_path: str):
        self.poll_timer.stop()
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
        self.progress_label.setText(f"{current}/{int(total)}")

    def _poll_output(self):
        """
        Poll the preview file during rendering
        """
        if self.worker:
            preview = Path(self.worker.preview_path)

            if preview.exists() and preview.stat().st_size > 0:
                elapsed = time.time() - self.start_time
                self._set_status_text(f"Rendering... {elapsed:.1f}s")
                self.display.update_from_file(str(preview))

    def _set_idle(self, message: str):
        self.render_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self._set_status_text(message)
