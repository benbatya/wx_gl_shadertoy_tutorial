#pragma once

#include <GL/glew.h> // must be included before glcanvas.h
#include <wx/glcanvas.h>
#include <wx/wx.h>

#include <chrono>

#include "osm_loader.h"
#include "shaderprogram.h"
#include <unordered_map>

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

    // Upload routes from OSMLoader into GPU buffers. This replaces the
    // existing VBO_/EBO_ contents when called.
    void SetRoutes(const OSMLoader::Routes &routes, const osmium::Box &bounds);

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

    // Coordinate bounds of viewport
    osmium::Box bounds_{};

    // Stored routes (kept so buffers can be uploaded after GL init)
    OSMLoader::Routes storedRoutes_{};
    // Draw commands: pair<count, byteOffsetInEBO>
    std::vector<std::pair<GLsizei, size_t>> drawCommands_{};

    // Update GPU buffers from `storedRoutes_` (called after GL init or when
    // SetRoutes is invoked while GL is available).
    void UpdateBuffersFromRoutes();
};