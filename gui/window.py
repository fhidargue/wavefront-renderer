import sys
import time
from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QFontMetrics
from PySide6.QtWidgets import (
    QComboBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QProgressBar,
    QPushButton,
    QSizePolicy,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from gui.render.compare_tab import CompareTab
from gui.render.display import RenderDisplay
from gui.render.results_tab import ResultsTab
from gui.render.worker import RenderWorker

PROJECT_ROOT = Path(__file__).resolve().parents[1]

CAMERA_PATH = str(PROJECT_ROOT / "scenes" / "cameras" / "cornellBoxCamera.usda")
KITCHEN_CAMERA_PATH = str(PROJECT_ROOT / "scenes" / "cameras" / "kitchenSetCamera.usda")

KNOWN_SCENE_CAMERAS = {
    "kitchenSet.usda": KITCHEN_CAMERA_PATH,
}

AVAILABLE_POLICIES = ["none", "material", "texture", "costBenefit"]


def camel_to_display(name: str) -> str:
    """
    Converts a camelCase or PascalCase string to a space-separated title case label.

    Args:
        name: camelCase or PascalCase string to convert.
    """
    import re

    spaced = re.sub(r"([A-Z])", r" \1", name).strip()
    return spaced.title()


def discover_scenes() -> dict:
    """
    Discovers all .usda scene files in the scenes/ directory dynamically.

    Returns:
        Dict mapping scene key to (scene_path, output_path, camera_path, scene_key).
    """
    scenes_dir = PROJECT_ROOT / "scenes"
    output_dir = PROJECT_ROOT / "output"
    scenes = {}

    for usda_file in sorted(scenes_dir.glob("*.usda")):
        scene_key = usda_file.stem
        camera_path = KNOWN_SCENE_CAMERAS.get(usda_file.name, CAMERA_PATH)
        output_path = str(output_dir / f"{scene_key}.exr")
        scenes[scene_key] = (
            str(usda_file),
            output_path,
            camera_path,
            scene_key,
            camel_to_display(scene_key),
        )

    return scenes


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
        memory_stats: bool = False,
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
        self.current_scene_key = "cornellBox"
        self.available_scenes = discover_scenes()
        self.memory_stats = memory_stats

        self.setWindowTitle("Wavefront Renderer")
        self._build_ui()
        self._build_poll_timer()

    def _build_ui(self):
        """
        Builds the main window UI with a render display, progress bar, controls, and results tab.
        """
        self.root_tabs = QTabWidget()
        self.setCentralWidget(self.root_tabs)

        render_widget = QWidget()
        layout = QVBoxLayout(render_widget)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)

        self.display = RenderDisplay(self.width, self.height)

        display_container = QHBoxLayout()
        display_container.addStretch()
        display_container.addWidget(self.display)
        display_container.addStretch()
        layout.addLayout(display_container)
        layout.addSpacing(8)

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        self.progress_bar.setFixedHeight(6)
        self.progress_bar.setTextVisible(False)
        layout.addWidget(self.progress_bar)

        controls = QHBoxLayout()
        controls.setSpacing(6)

        self.scene_combo = QComboBox()

        for scene_key, scene_data in self.available_scenes.items():
            display_name = scene_data[4]
            self.scene_combo.addItem(display_name, userData=scene_key)

        self.scene_combo.currentIndexChanged.connect(self._on_scene_changed)
        self.scene_combo.setFixedHeight(32)

        self.policy_combo = QComboBox()

        for policy in AVAILABLE_POLICIES:
            self.policy_combo.addItem(camel_to_display(policy), userData=policy)

        self.policy_combo.setFixedHeight(32)

        self.render_btn = QPushButton("Render")
        self.render_btn.setFixedHeight(32)
        self.render_btn.clicked.connect(self.start_render)

        self.stop_btn = QPushButton("Stop")
        self.stop_btn.setFixedHeight(32)
        self.stop_btn.setEnabled(False)
        self.stop_btn.clicked.connect(self.stop_render)

        controls.addWidget(QLabel("Scene:"))
        controls.addWidget(self.scene_combo)
        controls.addWidget(QLabel("Policy:"))
        controls.addWidget(self.policy_combo)
        controls.addWidget(self.render_btn)
        controls.addWidget(self.stop_btn)
        controls.addStretch()
        layout.addLayout(controls)

        status_row = QHBoxLayout()

        self.status_label = QLabel("Press Render to start")
        self.status_label.setStyleSheet("color: grey; font-size: 12px;")
        self.status_label.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        self.status_label.setMinimumWidth(0)

        self.progress_label = QLabel("")
        self.progress_label.setStyleSheet("color: grey; font-size: 12px;")
        self.progress_label.setAlignment(Qt.AlignmentFlag.AlignRight)

        status_row.addWidget(self.status_label, 1)
        status_row.addWidget(self.progress_label)
        layout.addLayout(status_row)

        self.root_tabs.addTab(render_widget, "Render")

        self.results_tab = ResultsTab()
        self.root_tabs.addTab(self.results_tab, "Results")

        self.compare_tab = CompareTab()
        self.root_tabs.addTab(self.compare_tab, "Compare")

        self.resize(self.width + 32, self.height + 120)

    def _build_poll_timer(self):
        """
        Builds a QTimer to poll the preview image from the renderer output.
        """
        self.poll_timer = QTimer()
        self.poll_timer.setInterval(500)
        self.poll_timer.timeout.connect(self._poll_output)

    def _on_scene_changed(self, index: int):
        """
        Handles the scene change event.

        Args:
            index: The index of the selected scene in the combo box.
        """
        scene_key = self.scene_combo.itemData(index)

        if scene_key not in self.available_scenes:
            return

        scene_path, output_path, camera_path, scene_key, _ = self.available_scenes[scene_key]
        self.scene_path = scene_path
        self.output_path = output_path
        self.camera_path = camera_path
        self.current_scene_key = scene_key

    def keyPressEvent(self, event):
        """
        Handles key press events. Closes the window on Escape key.

        Args:
            event: The key press event.
        """
        if event.key() == Qt.Key.Key_Escape:
            self.close()
        else:
            super().keyPressEvent(event)

    def start_render(self):
        """
        Starts the rendering process.
        """
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
            policy=self.policy_combo.currentData(),
            memory_stats=self.memory_stats,
        )
        self.worker.statusUpdate.connect(self._on_status_update)
        self.worker.progressUpdate.connect(self._on_progress_update)
        self.worker.renderComplete.connect(self._on_render_complete)
        self.worker.renderFailed.connect(self._on_render_failed)
        self.worker.outputCaptured.connect(self._on_output_captured)
        self.worker.start()

        self.poll_timer.start()

    def stop_render(self):
        """
        Stops the rendering process.
        """
        if self.worker:
            self.worker.stop()
        self.poll_timer.stop()
        self._set_idle("Stopped")

    def _set_status_text(self, text: str):
        """
        Elides long status text with ellipsis instead of forcing the window wider.

        Args:
            text: The text to display.
        """
        metrics = QFontMetrics(self.status_label.font())
        elided = metrics.elidedText(text, Qt.TextElideMode.ElideMiddle, self.status_label.width())
        self.status_label.setText(elided)

    def _on_status_update(self, line: str):
        """
        Updates the status text with a new line from the renderer.

        Args:
            line: A line of text from the renderer stdout.
        """
        if line.strip():
            self._set_status_text(line.strip())

    def _on_render_complete(self, elapsed_ms: float, output_path: str):
        """
        Handles the completion of the rendering process.

        Args:
            elapsed_ms: The time taken to complete the rendering in milliseconds.
            output_path: The path to the rendered output file.
        """
        self.poll_timer.stop()
        self.display.update_from_file(output_path)
        self.progress_bar.setValue(100)
        self._set_idle(f"Done. {elapsed_ms / 1000:.1f}s | {output_path}")

    def _on_render_failed(self, error: str):
        """
        Handles the failure of the rendering process.

        Args:
            error: The error message from the renderer.
        """
        self.poll_timer.stop()
        self._set_idle(f"Error: {error}")
        print(f"Render failed: {error}")

    def _on_progress_update(self, current: int, total: float):
        """
        Updates the progress bar and label.

        Args:
            current: The current sample count.
            total: The total sample count.
        """
        percent = int((current / total) * 100)
        self.progress_bar.setValue(percent)
        self.progress_label.setText(f"{current}/{int(total)}")

    def _on_output_captured(self, full_stdout: str):
        """
        Records benchmark data to CSV and refreshes the results tab.

        Args:
            full_stdout: The full stdout output from the renderer process.
        """
        try:
            import importlib.util
            import os

            os.chdir(str(PROJECT_ROOT))

            parse_results_path = PROJECT_ROOT / "scripts" / "results" / "parse_results.py"
            scripts_results_path = str(parse_results_path.parent)

            if scripts_results_path not in sys.path:
                sys.path.insert(0, scripts_results_path)

            spec = importlib.util.spec_from_file_location("parse_results", parse_results_path)
            parse_results_module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(parse_results_module)

            row = parse_results_module.parse(
                stdout=full_stdout,
                scene=self.current_scene_key,
                policy=self.policy_combo.currentData(),
            )
            parse_results_module.append_row(row)

            self._set_status_text(
                f"Recorded: scene={self.current_scene_key} "
                f"policy={self.policy_combo.currentData()} "
                f"shade_ms={row['shade_ms']}"
            )

            self.results_tab.refresh()
            self.compare_tab._populate_combos()

        except (OSError, RuntimeError, ImportError) as error:
            print(f"Failed to record benchmark data: {error}")

    def _poll_output(self):
        """
        Polls the preview file during rendering and updates the display.
        """
        if self.worker:
            preview = Path(self.worker.preview_path)
            if preview.exists() and preview.stat().st_size > 0:
                elapsed = time.time() - self.start_time
                self._set_status_text(f"Rendering... {elapsed:.1f}s")
                self.display.update_from_file(str(preview))

    def _set_idle(self, message: str):
        """
        Sets the window to an idle state after rendering completes or stops.

        Args:
            message: The status message to display.
        """
        self.render_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self._set_status_text(message)
