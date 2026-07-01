"""
Displays the rendered image as a fullscreen OpenGL texture.

Inspired by Jon Macey, PBORender and SimpleFBO demos, Bournemouth University
Code referenced from: https://github.com/NCCA/FBODemos
Date accessed: 28, June 2026.
"""

import numpy as np
from pathlib import Path
import ctypes
from PySide6.QtOpenGLWidgets import QOpenGLWidget
import OpenGL.GL as GL

from gui.image_loader import load_exr, linear_to_srgb


def _load_shader(filename: str) -> str:
    shader_path = Path(__file__).parent / "shaders" / filename

    if not shader_path.exists():
        raise FileNotFoundError(f"Shader not found: {shader_path}")

    return shader_path.read_text(encoding="utf-8")


def _compile_shader(source: str, shader_type: int) -> int:
    shader = GL.glCreateShader(shader_type)
    GL.glShaderSource(shader, source)
    GL.glCompileShader(shader)

    if not GL.glGetShaderiv(shader, GL.GL_COMPILE_STATUS):
        log = GL.glGetShaderInfoLog(shader).decode()
        raise RuntimeError(f"Shader compile error:\n{log}")

    return shader


def _create_program(vert_src: str, frag_src: str) -> int:
    vertex = _compile_shader(vert_src, GL.GL_VERTEX_SHADER)
    fragment = _compile_shader(frag_src, GL.GL_FRAGMENT_SHADER)

    program = GL.glCreateProgram()
    GL.glAttachShader(program, vertex)
    GL.glAttachShader(program, fragment)
    GL.glLinkProgram(program)

    if not GL.glGetProgramiv(program, GL.GL_LINK_STATUS):
        log = GL.glGetProgramInfoLog(program).decode()
        raise RuntimeError(f"Program link error:\n{log}")

    GL.glDeleteShader(vertex)
    GL.glDeleteShader(fragment)

    return program


class RenderDisplay(QOpenGLWidget):
    def __init__(self, width: int, height: int, parent=None):
        super().__init__(parent)
        self.render_width = width
        self.render_height = height
        self.texture_id = None
        self.vao = None
        self.vbo = None
        self.program = None
        self.has_image = False
        self.setMinimumSize(width, height)
        self.setMaximumSize(width, height)

    def initializeGL(self):
        GL.glClearColor(0.05, 0.05, 0.05, 1.0)

        self._init_shader()
        self._init_quad()
        self._init_texture()

    def _init_shader(self):
        vertex_shader = _load_shader("fullscreen.vert")
        fragment_shader = _load_shader("fullscreen.frag")

        self.program = _create_program(vertex_shader, fragment_shader)

    def _init_quad(self):
        # Fullscreen quad positions + texture coords
        vertices = np.array(
            [
                -1.0,
                -1.0,
                0.0,
                0.0,
                1.0,
                -1.0,
                1.0,
                0.0,
                1.0,
                1.0,
                1.0,
                1.0,
                -1.0,
                -1.0,
                0.0,
                0.0,
                1.0,
                1.0,
                1.0,
                1.0,
                -1.0,
                1.0,
                0.0,
                1.0,
            ],
            dtype=np.float32,
        )

        self.vao = GL.glGenVertexArrays(1)
        self.vbo = GL.glGenBuffers(1)

        GL.glBindVertexArray(self.vao)
        GL.glBindBuffer(GL.GL_ARRAY_BUFFER, self.vbo)
        GL.glBufferData(
            GL.GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL.GL_STATIC_DRAW
        )

        stride = 4 * vertices.itemsize

        GL.glVertexAttribPointer(
            0, 2, GL.GL_FLOAT, GL.GL_FALSE, stride, ctypes.c_void_p(0)
        )
        GL.glEnableVertexAttribArray(0)

        GL.glVertexAttribPointer(
            1,
            2,
            GL.GL_FLOAT,
            GL.GL_FALSE,
            stride,
            ctypes.c_void_p(2 * vertices.itemsize),
        )
        GL.glEnableVertexAttribArray(1)

        GL.glBindVertexArray(0)

    def _init_texture(self):
        self.texture_id = GL.glGenTextures(1)

        GL.glBindTexture(GL.GL_TEXTURE_2D, self.texture_id)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MAG_FILTER, GL.GL_LINEAR)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP_TO_EDGE)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP_TO_EDGE)

        # Allocate empty texture storage
        GL.glTexImage2D(
            GL.GL_TEXTURE_2D,
            0,
            GL.GL_RGB32F,
            self.render_width,
            self.render_height,
            0,
            GL.GL_RGB,
            GL.GL_FLOAT,
            None,
        )

    def update_from_file(self, filepath: str):
        """
        Load an EXR from disk and upload to the GPU texture
        """
        if self.texture_id is None:
            return
        
        # If the file fails mid write, skip the frame
        try:
            pixels = load_exr(filepath)

            if pixels is None:
                return
        except Exception:
            return 

        self._upload_pixels(linear_to_srgb(pixels))
        self.has_image = True
        self.update()

    def linear_to_srgb(pixels: np.ndarray) -> np.ndarray:
        pixels = np.clip(pixels, 0.0, 1.0)

        return np.power(pixels, 1.0 / 2.2).astype(np.float32)

    def _upload_pixels(self, pixels: np.ndarray):
        """
        Upload numpy array to GPU texture
        """
        if self.texture_id is None:
            return
        
        height, width, _ = pixels.shape
        if width != self.render_width or height != self.render_height:
            return
        
        self.makeCurrent()
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.texture_id)
        GL.glTexSubImage2D(
            GL.GL_TEXTURE_2D,
            0,
            0,
            0,
            width,
            height,
            GL.GL_RGB,
            GL.GL_FLOAT,
            pixels.flatten(),
        )
        self.doneCurrent()

    def paintGL(self):
        GL.glClear(GL.GL_COLOR_BUFFER_BIT)

        if not self.has_image:
            return

        GL.glUseProgram(self.program)
        GL.glActiveTexture(GL.GL_TEXTURE0)
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.texture_id)
        GL.glUniform1i(GL.glGetUniformLocation(self.program, "renderTexture"), 0)

        GL.glBindVertexArray(self.vao)
        GL.glDrawArrays(GL.GL_TRIANGLES, 0, 6)
        GL.glBindVertexArray(0)

        GL.glUseProgram(0)

    def resizeGL(self, w: int, h: int):
        GL.glViewport(0, 0, w, h)
