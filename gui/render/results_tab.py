import importlib.util
import sys
from pathlib import Path

import seaborn as sns
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
from PySide6.QtWidgets import (
    QHBoxLayout,
    QLabel,
    QPushButton,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

RESULTS_DIR = Path(__file__).resolve().parents[2] / "results"

FIGURE_TAB_LABELS = {
    "shade_time": "Shade Time",
    "pipeline": "Pipeline",
    "run_length": "Run Length",
    "homogeneity": "Homogeneity",
}


class ResultsTab(QWidget):
    """
    Embeds benchmark result figures inside the GUI as a tab.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)

        top_bar = QHBoxLayout()
        self.status_label = QLabel("No data yet. Refresh or run a benchmark to see results.")
        self.status_label.setStyleSheet("color: grey; font-size: 12px;")

        refresh_btn = QPushButton("Refresh")
        refresh_btn.setFixedHeight(32)
        refresh_btn.clicked.connect(self.refresh)

        top_bar.addWidget(self.status_label, 1)
        top_bar.addWidget(refresh_btn)
        layout.addLayout(top_bar)

        self.sample_tabs = QTabWidget()
        layout.addWidget(self.sample_tabs)

    def refresh(self):
        """
        Discovers all per sample CSV files, builds figures for each,
        and populates one outer tab per sample count.
        """
        bucket_files = sorted(
            RESULTS_DIR.glob("benchmark_results_*.csv"),
            key=lambda p: int(p.stem.replace("benchmark_results_", "")),
        )

        if not bucket_files:
            self.status_label.setText("No benchmark data found. Run a stress scene first.")
            return

        try:
            plot_results_module = self._load_plot_results()
            sns.set_theme(style="whitegrid", palette="tab10")

            self.sample_tabs.blockSignals(True)

            while self.sample_tabs.count() > 0:
                self.sample_tabs.removeTab(0)

            loaded_count = 0

            for csv_path in bucket_files:
                sample_label = csv_path.stem.replace("benchmark_results_", "")
                df = plot_results_module.load_data(csv_path)

                if df.empty:
                    continue

                figures = plot_results_module.build_figures(df)
                sample_widget = self._build_sample_tab(figures)
                self.sample_tabs.addTab(sample_widget, f"{sample_label} samples")
                loaded_count += 1

            self.sample_tabs.blockSignals(False)

            if loaded_count == 0:
                self.status_label.setText("CSV files found but contain no valid data.")
            else:
                self.status_label.setText(
                    f"Showing {loaded_count} sample bucket(s) — "
                    f"{', '.join(f.stem.replace('benchmark_results_', '') for f in bucket_files)} samples."
                )

        except (OSError, RuntimeError, KeyError, ValueError) as error:
            self.status_label.setText(f"Error loading results: {error}")

    def _build_sample_tab(self, figures: dict) -> QWidget:
        """
        Builds a widget containing inner tabs for each figure in the sample group.

        Args:
            figures: Dict of figure name to matplotlib Figure from build_figures().
        """
        widget = QWidget()
        layout = QVBoxLayout(widget)
        layout.setContentsMargins(0, 4, 0, 0)

        figure_tabs = QTabWidget()
        figure_tabs.setDocumentMode(True)

        for key, label in FIGURE_TAB_LABELS.items():
            if key not in figures:
                continue
            canvas = FigureCanvasQTAgg(figures[key])
            figure_tabs.addTab(canvas, label)

        layout.addWidget(figure_tabs)
        return widget

    def _load_plot_results(self):
        """
        Loads plot_results.py via importlib to avoid sys.path conflicts
        """
        plot_results_path = RESULTS_DIR.parent / "scripts" / "results" / "plot_results.py"
        scripts_results_path = str(plot_results_path.parent)

        if scripts_results_path not in sys.path:
            sys.path.insert(0, scripts_results_path)

        spec = importlib.util.spec_from_file_location("plot_results", plot_results_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)

        return module
