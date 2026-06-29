import os
import time
import subprocess

from pathlib import Path

from PySide6.QtCore import QThread, Signal


class RenderWorker(QThread):
    """
    Runs the compiled C++ renderer in a background thread
    """

    statusUpdate = Signal(str)
    progressUpdate = Signal(int, float)
    renderComplete = Signal(float, str)
    renderFailed = Signal(str)

    def __init__(
        self,
        renderer_path: str,
        scene_path: str,
        output_path: str,
        parent=None,
    ):
        super().__init__(parent)
        self.renderer_path = Path(renderer_path)
        self.scene_path = scene_path
        self.output_path = output_path
        self.preview_path = self._derive_preview_path(output_path)
        self._stop = False

    @staticmethod
    def _derive_preview_path(output_path: str) -> str:
        """
        Matches the C++ preview path logic: insert _preview before extension
        """
        path = Path(output_path)

        return str(path.parent / f"{path.stem}_preview{path.suffix}")

    def stop(self):
        """
        Request the render to terminate the subprocess
        """
        self._stop = True

    def run(self):
        """
        Runs on the background thread
        """
        # Remove stale preview from previous render
        preview = Path(self.preview_path)

        if preview.exists():
            try:
                os.remove(preview)
            except OSError:
                pass

        if not self.renderer_path.exists():
            self.renderFailed.emit(
                f"Renderer not found: {self.renderer_path}\nRun 'make build' first."
            )
            return

        start_ms = time.time() * 1000

        cmd = [
            str(self.renderer_path),
            self.scene_path,
            self.output_path,
        ]

        try:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True,
            )

            for line in process.stdout:
                if self._stop:
                    process.terminate()
                    return

                line = line.rstrip()

                if line.startswith("Sample:"):
                    try:
                        parts = line.split()
                        current, total = parts[1].split("/")
                        self.progressUpdate.emit(int(current), float(total))
                    except (IndexError, ValueError):
                        pass
                    continue
                elif line.startswith("Image written:"):
                    continue

                print(line)
                self.statusUpdate.emit(line)

            process.wait()
            elapsed = time.time() * 1000 - start_ms

            if process.returncode != 0:
                self.renderFailed.emit(
                    f"Renderer exited with code {process.returncode}"
                )
                return

            self.renderComplete.emit(elapsed, self.output_path)

            # Clean up preview file
            preview = Path(self.preview_path)

            if preview.exists():
                try:
                    os.remove(preview)
                except OSError as e:
                    print(f"Could not delete preview: {e}")

        except Exception as e:
            self.renderFailed.emit(str(e))
