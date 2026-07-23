import os
import sys
from pathlib import Path

from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QSurfaceFormat

from gui.window import RenderWindow


def _parse_flag_value(argv: list[str], flag: str, default: bool) -> bool:
    """
    Parses flag command arguments
    """
    if flag not in argv:
        return default

    flag_index = argv.index(flag)

    if flag_index + 1 < len(argv):
        return argv[flag_index + 1] == "1"

    return default


def configure_opengl():
    """
    Set OpenGL format before creating QApplication
    """
    fmt = QSurfaceFormat()
    fmt.setVersion(3, 3)
    fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
    fmt.setDepthBufferSize(24)
    QSurfaceFormat.setDefaultFormat(fmt)


def resolve_paths(
    argv: list[str],
) -> tuple[str, str, str, str, int, int, bool, str, bool, bool, int | None, bool]:
    project_root = Path(__file__).resolve().parents[1]
    renderer_path = str(project_root / "build" / "renderer")
    scene_path = (
        argv[1] if len(argv) > 1 else str(project_root / "scenes" / "cornellBox.usda")
    )
    output_path = (
        argv[2] if len(argv) > 2 else str(project_root / "output" / "preview.exr")
    )
    camera_path = argv[3] if len(argv) > 3 else ""

    # Denoise flag
    width = int(os.environ.get("WIDTH", 600))
    height = int(os.environ.get("HEIGHT", 600))
    denoise = "--denoise" in argv

    # Env HDRI flag
    env_path = ""
    if "--env" in argv:
        env_index = argv.index("--env")
        if env_index + 1 < len(argv):
            env_path = argv[env_index + 1]

    cost_rr = _parse_flag_value(argv, "--cost-rr", default=True)
    ray_sort = _parse_flag_value(argv, "--ray-sort", default=True)

    samples = None

    if "--samples" in argv:
        samples_index = argv.index("--samples")

        if samples_index + 1 < len(argv):
            samples = int(argv[samples_index + 1])

    adaptive_sampling = "--no-adaptive" not in argv

    return (
        renderer_path,
        scene_path,
        output_path,
        camera_path,
        width,
        height,
        denoise,
        env_path,
        cost_rr,
        ray_sort,
        samples,
        adaptive_sampling,
    )


def main():
    configure_opengl()

    app = QApplication(sys.argv)
    app.setApplicationName("Wavefront Renderer")
    app.setOrganizationName("Bournemouth University")

    (
        renderer_path,
        scene_path,
        output_path,
        camera_path,
        width,
        height,
        denoise,
        env_path,
        cost_rr,
        ray_sort,
        samples,
        adaptive_sampling,
    ) = resolve_paths(sys.argv)

    window = RenderWindow(
        renderer_path=renderer_path,
        scene_path=scene_path,
        output_path=output_path,
        camera_path=camera_path,
        width=width,
        height=height,
        denoise=denoise,
        env_path=env_path,
        cost_rr=cost_rr,
        ray_sort=ray_sort,
        samples=samples,
        adaptive_sampling=adaptive_sampling,
    )
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
