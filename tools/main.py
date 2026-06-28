import sys
from pathlib import Path

from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QSurfaceFormat

from tools.window import RenderWindow


def configure_opengl():
    """
    Set OpenGL format before creating QApplication
    """
    fmt = QSurfaceFormat()
    fmt.setVersion(3, 3)
    fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
    fmt.setDepthBufferSize(24)
    QSurfaceFormat.setDefaultFormat(fmt)


def resolve_paths(argv: list[str]) -> tuple[str, str, str]:
    """
    Resolve renderer, scene and output paths from CLI args or defaults
    """
    project_root = Path(__file__).resolve().parents[1]
    renderer_path = str(project_root / "build" / "renderer")
    scene_path = (
        argv[1] if len(argv) > 1 else str(project_root / "scenes" / "cornellBox.usda")
    )
    output_path = (
        argv[2] if len(argv) > 2 else str(project_root / "output" / "preview.exr")
    )

    return renderer_path, scene_path, output_path


def main():
    configure_opengl()

    app = QApplication(sys.argv)
    app.setApplicationName("Wavefront Renderer")
    app.setOrganizationName("Bournemouth University")

    renderer_path, scene_path, output_path = resolve_paths(sys.argv)

    window = RenderWindow(
        renderer_path=renderer_path,
        scene_path=scene_path,
        output_path=output_path,
        width=600,
        height=600,
    )
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
