#pragma once

#include <GL/glew.h> // must be included before glcanvas.h
#include <wx/glcanvas.h>
#include <wx/wx.h>

#include <chrono>

#include "shaderprogram.h"

wxDECLARE_EVENT(wxEVT_OPENGL_INITIALIZED, wxCommandEvent);

class OpenGLCanvas : public wxGLCanvas {
  public:
    OpenGLCanvas(wxWindow *parent, const wxGLAttributes &canvasAttrs);
    ~OpenGLCanvas();

    bool InitializeOpenGL();

    bool IsOpenGLInitialized() const { return isOpenGLInitialized; }

    void OnPaint(wxPaintEvent &event);
    void OnSize(wxSizeEvent &event);

    void OnTimer(wxTimerEvent &event);

  protected:
    void CompileShaderProgram();

    std::string GetShaderBuildLog() const {
        return shaderProgram.lastBuildLog.str();
    }

  private:
    bool InitializeOpenGLFunctions();

    wxGLContext *openGLContext;
    bool isOpenGLInitialized{false};

    ShaderProgram shaderProgram{};

    wxTimer timer;
    std::chrono::high_resolution_clock::time_point openGLInitializationTime{};
    float elapsedSeconds{0.0f};

    GLuint VAO_{0};
    GLuint VBO_{0};           // vertex buffer object
    GLuint EBO_{0};           // element buffer object
    GLsizei elementCount_{0}; // number of indices in the EBO
};