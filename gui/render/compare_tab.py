import sys
from pathlib import Path

import numpy as np
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
from matplotlib.figure import Figure
from PySide6.QtWidgets import (
    QComboBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSizePolicy,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

OUTPUT_DIR = Path(__file__).resolve().parents[2] / "output"


class CompareTab(QWidget):
    """
    Heatmap comparison tool for rendered EXR images.
    Loads two images, computes luminance difference, and displays as a heatmap.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)

        # Image selection row
        controls = QHBoxLayout()
        controls.setSpacing(6)

        self.image_a_combo = QComboBox()
        self.image_b_combo = QComboBox()
        self.image_a_combo.setFixedHeight(32)
        self.image_a_combo.setMaximumWidth(220)
        self.image_b_combo.setFixedHeight(32)
        self.image_b_combo.setMaximumWidth(220)

        combos_row = QHBoxLayout()
        combos_row.setSpacing(6)
        combos_row.addWidget(QLabel("Image A:"))
        combos_row.addWidget(self.image_a_combo, 1)
        combos_row.addWidget(QLabel("Image B:"))
        combos_row.addWidget(self.image_b_combo, 1)
        layout.addLayout(combos_row)

        buttons_row = QHBoxLayout()
        buttons_row.setSpacing(6)
        compare_btn = QPushButton("Compare")
        compare_btn.setFixedHeight(32)
        compare_btn.clicked.connect(self._on_compare)
        refresh_btn = QPushButton("Refresh")
        refresh_btn.setFixedHeight(32)
        refresh_btn.clicked.connect(self._populate_combos)

        buttons_row.addStretch()
        buttons_row.addWidget(compare_btn)
        buttons_row.addWidget(refresh_btn)
        layout.addLayout(buttons_row)

        buttons_row.addWidget(compare_btn)
        buttons_row.addWidget(refresh_btn)
        buttons_row.addStretch()
        layout.addLayout(buttons_row)

        # Matplotlib canvas for heatmap
        self.content_tabs = QTabWidget()
        self.content_tabs.setDocumentMode(True)
        self.content_tabs.setMinimumSize(0, 0)
        layout.addWidget(self.content_tabs, stretch=1)

        # Status label
        self.status_label = QLabel(
            "Select two rendered images and click Compare to see the luminance difference heatmap."
        )
        self.status_label.setStyleSheet("color: grey; font-size: 12px;")
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

        self._populate_combos()

    def _populate_combos(self):
        """
        Scans the output directory for EXR files excluding previews
        and populates both dropdowns.
        """
        exr_files = sorted(f for f in OUTPUT_DIR.glob("*.exr") if "_preview" not in f.name)

        current_a = self.image_a_combo.currentText()
        current_b = self.image_b_combo.currentText()

        self.image_a_combo.clear()
        self.image_b_combo.clear()

        for exr in exr_files:
            self.image_a_combo.addItem(exr.name, userData=str(exr))
            self.image_b_combo.addItem(exr.name, userData=str(exr))

        # Restore previous selection if still available
        index_a = self.image_a_combo.findText(current_a)
        index_b = self.image_b_combo.findText(current_b)

        if index_a >= 0:
            self.image_a_combo.setCurrentIndex(index_a)
        if index_b >= 0:
            self.image_b_combo.setCurrentIndex(max(index_b, 1))

        count = self.image_a_combo.count()
        self.status_label.setText(
            f"{count} image(s) found in output/."
            if count > 0
            else "No EXR files found in output/. Run a render first."
        )

    def _load_exr(self, path: str) -> np.ndarray | None:
        """
        Loads an EXR file and returns it as a float32 numpy array (H, W, 3).

        Args:
            path: Absolute path to the EXR file.
        """
        try:
            sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
            from gui.image_loader import load_exr

            return load_exr(path)
        except (OSError, RuntimeError) as error:
            self.status_label.setText(f"Failed to load image: {error}")
            return None

    def _compute_luminance_diff(self, img_a: np.ndarray, img_b: np.ndarray) -> np.ndarray:
        """
        Computes per-pixel absolute luminance difference between two images
        using ITU-R BT.709 weights matching the renderer's adaptive sampler.

        Args:
            img_a: First image as float32 array (H, W, 3).
            img_b: Second image as float32 array (H, W, 3).
        """
        weights = np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
        lum_a = np.dot(img_a, weights)
        lum_b = np.dot(img_b, weights)

        return np.abs(lum_a - lum_b)

    def _on_compare(self):
        """
        Loads both selected images, computes the luminance difference heatmap,
        and renders it into the matplotlib canvas.
        """
        path_a = self.image_a_combo.currentData()
        path_b = self.image_b_combo.currentData()

        if not path_a or not path_b:
            self.status_label.setText("Select two images first.")
            return

        if path_a == path_b:
            self.status_label.setText("Select two different images.")
            return

        img_a = self._load_exr(path_a)
        img_b = self._load_exr(path_b)

        if img_a is None or img_b is None:
            return

        if img_a.shape != img_b.shape:
            self.status_label.setText(
                f"Image dimensions do not match: {img_a.shape} vs {img_b.shape}"
            )
            return

        diff = self._compute_luminance_diff(img_a, img_b)

        name_a = Path(path_a).name
        name_b = Path(path_b).name

        self.figure = Figure()
        self.figure.set_tight_layout(True)
        self.canvas = FigureCanvasQTAgg(self.figure)
        self.canvas.setMinimumSize(0, 0)
        self.canvas.setSizePolicy(
            QSizePolicy.Policy.Expanding,
            QSizePolicy.Policy.Expanding,
        )

        ax = self.figure.add_subplot(111)
        heatmap = ax.imshow(np.flipud(diff), cmap="inferno", aspect="auto")
        self.figure.colorbar(heatmap, ax=ax, label="Absolute luminance difference")
        ax.set_title(f"{name_a}  vs  {name_b}", fontsize=9)
        ax.axis("off")

        while self.content_tabs.count() > 0:
            self.content_tabs.removeTab(0)

        self.content_tabs.addTab(self.canvas, "Heatmap")
        self.canvas.draw()

        mean_diff = float(diff.mean())
        max_diff = float(diff.max())
        per_channel = np.abs(img_a - img_b).mean(axis=(0, 1))

        self.status_label.setText(
            f"Mean Avg: {mean_diff:.4f}. Average luminance difference across all pixels.\n"
            f"Max Avg: {max_diff:.4f}. Largest single-pixel difference.\n"
            f"Mean per channel: R {per_channel[0]:.4f}  G {per_channel[1]:.4f}  B {per_channel[2]:.4f}.\n"
            f"Bright areas in the heatmap indicate pixels where the two policies produced the most divergent results."
        )
