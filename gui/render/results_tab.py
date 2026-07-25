from pathlib import Path

from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
from PySide6.QtWidgets import (
    QHBoxLayout,
    QLabel,
    QPushButton,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

RESULTS_CSV = Path("results/benchmark_results.csv")


class ResultsTab(QWidget):
    """
    Embeds the benchmark result figures directly inside the GUI as a tab.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)

        # Top bar
        top_bar = QHBoxLayout()
        self.status_label = QLabel("No data yet. Refresh or run a benchmark to see results.")
        self.status_label.setStyleSheet("color: grey; font-size: 12px;")

        refresh_btn = QPushButton("Refresh")
        refresh_btn.setFixedHeight(32)
        refresh_btn.clicked.connect(self.refresh)

        top_bar.addWidget(self.status_label, 1)
        top_bar.addWidget(refresh_btn)
        layout.addLayout(top_bar)

        # Inner tab widget
        self.figure_tabs = QTabWidget()
        layout.addWidget(self.figure_tabs)

    def refresh(self):
        """
        Reloads the CSV and redraws all figures.
        Called automatically after each render completes.
        """
        if not RESULTS_CSV.exists():
            self.status_label.setText("No benchmark data found. Run a render first.")
            return

        try:
            import importlib.util
            import sys

            import seaborn as sns

            sns.set_theme(style="whitegrid", palette="tab10")

            plot_results_path = (
                Path(__file__).resolve().parents[2] / "scripts" / "results" / "plot_results.py"
            )
            spec = importlib.util.spec_from_file_location("plot_results", plot_results_path)
            plot_results_module = importlib.util.module_from_spec(spec)

            # Add scripts/results to sys.path so plot_results.py can find its constants
            scripts_results_path = str(plot_results_path.parent)
            if scripts_results_path not in sys.path:
                sys.path.insert(0, scripts_results_path)

            spec.loader.exec_module(plot_results_module)

            df = plot_results_module.load_data()

            if df.empty:
                self.status_label.setText("CSV exists but contains no data.")
                return

            figures = plot_results_module.build_figures(df)
            self._populate_tabs(figures)

            self.status_label.setText(
                f"Showing {len(df)} result rows. "
                f"{len(df['scene'].unique())} scenes, "
                f"{len(df['policy'].unique())} policies."
            )

        except (OSError, RuntimeError, KeyError, ValueError) as error:
            self.status_label.setText(f"Error loading results: {error}")

    def _populate_tabs(self, figures: dict):
        """
        Clears and repopulates the inner tab widget with one canvas per figure.

        Args:
            figures: Dict of name to matplotlib Figure from build_figures().
        """
        while self.figure_tabs.count() > 0:
            self.figure_tabs.removeTab(0)

        tab_labels = {
            "shade_time": "Shade Time",
            "pipeline": "Pipeline",
            "run_length": "Run Length",
            "homogeneity": "Homogeneity",
        }

        for key, label in tab_labels.items():
            if key not in figures:
                continue
            canvas = FigureCanvasQTAgg(figures[key])
            self.figure_tabs.addTab(canvas, label)
