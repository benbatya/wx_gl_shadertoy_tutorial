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

    bool IsOpenGLInitialized() const { return isOpenGLInitialized_; }

    void OnPaint(wxPaintEvent &event);
    void OnSize(wxSizeEvent &event);

    void OnTimer(wxTimerEvent &event);

    void OnLeftDown(wxMouseEvent &event);
    void OnLeftUp(wxMouseEvent &event);
    void OnMouseMotion(wxMouseEvent &event);
    void OnMouseWheel(wxMouseEvent &event);
    void OnZoomGesture(wxZoomGestureEvent &event);

    // Upload routes from OSMLoader into GPU buffers. This replaces the
    // existing VBO_/EBO_ contents when called.
    void SetWays(const OSMLoader::Id2Way &routes, const osmium::Box &bounds);

  protected:
    void CompileShaderProgram();

    std::string GetShaderBuildLog() const { return shaderProgram_.lastBuildLog_.str(); }

    bool InitializeOpenGLFunctions();

    // Update GPU buffers from `storedWays_` (called after GL init or when
    // SetWays is invoked while GL is available).
    void UpdateBuffersFromRoutes();

    void Zoom(double scale, const wxPoint &mousePos);

    // utility methods to convert from Viewport->OSM and OSM->Viewport
    osmium::Location mapViewport2OSM(const wxPoint &viewportCoord);
    wxPoint mapOSM2Viewport(const osmium::Location &coords);

  private:
    wxGLContext *openGLContext_;
    bool isOpenGLInitialized_{false};

    ShaderProgram shaderProgram_{};

    wxTimer timer_;
    std::chrono::high_resolution_clock::time_point openGLInitializationTime_{};
    float elapsedSeconds_{0.0f};

    // FPS display/state
    std::chrono::high_resolution_clock::time_point lastFpsUpdateTime_{};
    int framesSinceLastFps_{0};
    float fps_{0.0f};

    GLuint VAO_{0};
    GLuint VBO_{0};           // vertex buffer object
    GLuint EBO_{0};           // element buffer object
    GLsizei elementCount_{0}; // number of indices in the EBO

    // OSM Coordinate bounds
    osmium::Box coordinateBounds_{};

    // bounding box in viewport coordinate system
    wxSize viewportSize_{};
    wxRect viewportBounds_{};

    // Stored routes (kept so buffers can be uploaded after GL init)
    OSMLoader::Id2Way storedWays_{};
    // Draw commands: pair<count, byteOffsetInEBO>
    std::vector<std::pair<GLsizei, size_t>> drawCommands_{};

    // Event handling state
    // Mouse drag state for panning
    bool isDragging_{false};
    wxPoint lastMousePos_{0, 0};
    long prevEventTimestamp_{0};
    double lastZoomFactor_{1.0};
};