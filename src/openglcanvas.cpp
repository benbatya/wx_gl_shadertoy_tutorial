#include "openglcanvas.h"

#include <shaders.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

wxDEFINE_EVENT(wxEVT_OPENGL_INITIALIZED, wxCommandEvent);

// GL debug callback function used when KHR_debug is available. Logs
// messages (skips notifications) through wxLogError and stderr for
// high-severity messages.
static void GLDebugCallbackFunc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                const GLchar *message, const void *userParam) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }

    std::ostringstream ss;
    ss << "GL Debug (id=" << id << ") ";

    switch (source) {
    case GL_DEBUG_SOURCE_API:
        ss << "source=API ";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        ss << "source=WindowSystem ";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        ss << "source=ShaderCompiler ";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        ss << "source=ThirdParty ";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        ss << "source=Application ";
        break;
    case GL_DEBUG_SOURCE_OTHER:
    default:
        ss << "source=Other ";
        break;
    }

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        ss << "type=Error ";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        ss << "type=DeprecatedBehavior ";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        ss << "type=UndefinedBehavior ";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        ss << "type=Portability ";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        ss << "type=Performance ";
        break;
    case GL_DEBUG_TYPE_OTHER:
    default:
        ss << "type=Other ";
        break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        ss << "severity=HIGH ";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        ss << "severity=MEDIUM ";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        ss << "severity=LOW ";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    default:
        ss << "severity=NOTIFICATION ";
        break;
    }

    ss << "message=" << message;

    std::cerr << ss.str() << std::endl;
}

OpenGLCanvas::OpenGLCanvas(wxWindow *parent, const wxGLAttributes &canvasAttrs) : wxGLCanvas(parent, canvasAttrs) {
    wxGLContextAttrs ctxAttrs;
    ctxAttrs.PlatformDefaults().CoreProfile().OGLVersion(3, 3).EndList();
    openGLContext_ = new wxGLContext(this, nullptr, &ctxAttrs);

    if (!openGLContext_->IsOK()) {
        wxMessageBox("This sample needs an OpenGL 3.3 capable driver.", "OpenGL version error",
                     wxOK | wxICON_INFORMATION, this);
        delete openGLContext_;
        openGLContext_ = nullptr;
    }

    Bind(wxEVT_PAINT, &OpenGLCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &OpenGLCanvas::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &OpenGLCanvas::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &OpenGLCanvas::OnLeftUp, this);
    Bind(wxEVT_MOTION, &OpenGLCanvas::OnMouseMotion, this);

    // Bind mouse wheel events for zooming
    Bind(wxEVT_MOUSEWHEEL, &OpenGLCanvas::OnMouseWheel, this);

    // Bind zoom gesture events
    Bind(wxEVT_GESTURE_ZOOM, &OpenGLCanvas::OnZoomGesture, this);
    EnableTouchEvents(wxTOUCH_ZOOM_GESTURE);

    timer_.SetOwner(this);
    this->Bind(wxEVT_TIMER, &OpenGLCanvas::OnTimer, this);

    constexpr auto FPS = 60.0;
    timer_.Start(1000 / FPS);
}

void OpenGLCanvas::SetWays(const OSMLoader::Ways &ways, const osmium::Box &bounds) {
    bounds_ = bounds;
    // Find the longest ways and store only those for testing
    storedWays_.clear();

    // const size_t NUM_WAYS = std::min(ways.size(), static_cast<size_t>(1));

    // std::unordered_map<size_t, std::vector<osmium::object_id_type>> lenIdMap;

    // for (const auto &way : ways) {
    //     auto length = way.second.nodes.size();
    //     lenIdMap[length].push_back(way.first);
    // }

    // std::vector<size_t> sortedLengths;
    // for (const auto &pair : lenIdMap) {
    //     sortedLengths.push_back(pair.first);
    // }
    // std::sort(sortedLengths.rbegin(), sortedLengths.rend());

    // size_t count = 0;
    // for (size_t length : sortedLengths) {
    //     for (auto wayId : lenIdMap[length]) {
    //         if (count >= NUM_WAYS)
    //             break;
    //         storedWays_[wayId] = ways.at(wayId);
    //         std::cout << "Selected way ID " << wayId << ", '"
    //                   << storedWays_.at(wayId).name << "' with length "
    //                   << length << std::endl;
    //         ++count;
    //     }
    // }

    // take first N ways
    // size_t count = 0;
    // for (const auto &route : ways) {
    //     if (count >= NUM_WAYS) {
    //         break;
    //     }
    //     ++count;
    //     storedRoutes_[route.first] = route.second;
    // }

    // Take all ways
    storedWays_ = ways;

    if (isOpenGLInitialized_) {
        UpdateBuffersFromRoutes();
    }
}

void OpenGLCanvas::UpdateBuffersFromRoutes() {
    // Build vertex and index arrays from storedRoutes_. Vertex layout:
    // x,y,r,g,b
    std::vector<float> vertices;
    std::vector<GLuint> indices;
    drawCommands_.clear();

    if (storedWays_.empty()) {
        elementCount_ = 0;
        return;
    }

    using FLOAT_VEC3 = std::array<GLfloat, 3>;
    using MapType = std::unordered_map<std::string, FLOAT_VEC3>;
    static MapType HIGHWAY2COLOR = {{"motorway", {1.0f, 0.35f, 0.35f}},   {"motorway_link", {1.0f, 0.6f, 0.6f}},
                                    {"secondary", {1.0f, 0.75f, 0.4f}},   {"tertiary", {1.0f, 1.0f, 0.6f}},
                                    {"residential", {1.0f, 1.0f, 1.0f}},  {"unclassified", {0.95f, 0.95f, 0.95f}},
                                    {"service", {0.8f, 0.8f, 0.8f}},      {"track", {0.65f, 0.55f, 0.4f}},
                                    {"pedestrian", {0.85f, 0.8f, 0.85f}}, {"footway", {0.9f, 0.7f, 0.7f}},
                                    {"path", {0.6f, 0.7f, 0.6f}},         {"steps", {0.7f, 0.4f, 0.4f}},
                                    {"platform", {0.6f, 0.6f, 0.8f}}};
    FLOAT_VEC3 DEFAULT_COLOR = {0.5f, 0.5f, 0.5f};

    size_t indexOffset = 0;
    for (const auto &entry : storedWays_) {
        const auto &coords = entry.second;
        if (coords.nodes.size() < 2)
            continue;

        GLuint base = static_cast<GLuint>(vertices.size() / 5);
        const auto &color =
            HIGHWAY2COLOR.count(entry.second.type) == 0 ? DEFAULT_COLOR : HIGHWAY2COLOR.at(entry.second.type);

        for (const auto &loc : coords.nodes) {
            assert(loc.valid());
            double lon = loc.lon();
            double lat = loc.lat();
            // store raw lon/lat in vertex attributes; shader will normalize
            vertices.push_back(static_cast<float>(lon));
            vertices.push_back(static_cast<float>(lat));
            vertices.push_back(color[0]);
            vertices.push_back(color[1]);
            vertices.push_back(color[2]);
        }

        // indices for GL_LINE_STRIP_ADJACENCY: duplicate first and last
        GLuint countHere = 0;
        // start: duplicate first
        indices.push_back(base);
        countHere += 1;

        for (size_t i = 0; i < (vertices.size() / 5) - base; ++i) {
            indices.push_back(base + static_cast<GLuint>(i));
            ++countHere;
        }

        // duplicate last
        indices.push_back(base + static_cast<GLuint>((vertices.size() / 5) - 1 - base));
        countHere += 1;

        // record draw command (count, byte offset)
        size_t startByteOffset = indexOffset * sizeof(GLuint);
        drawCommands_.emplace_back(static_cast<GLsizei>(countHere), startByteOffset);
        indexOffset += countHere;
    }

    elementCount_ = static_cast<GLsizei>(indices.size());

    // Create VAO/VBO/EBO if necessary and upload
    if (VAO_ == 0)
        glGenVertexArrays(1, &VAO_);
    glBindVertexArray(VAO_);

    if (VBO_ == 0)
        glGenBuffers(1, &VBO_);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    if (!vertices.empty())
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    if (EBO_ == 0)
        glGenBuffers(1, &EBO_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
    if (!indices.empty())
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void OpenGLCanvas::CompileShaderProgram() {
    shaderProgram_.vertexShaderSource_ = VertexShader;
    shaderProgram_.geometryShaderSource_ = GeometryShader;
    shaderProgram_.fragmentShaderSource_ = FragmentShader;
    shaderProgram_.Build();

    if (!GetShaderBuildLog().empty()) {
        std::cerr << "Shader failed to compile." << std::endl;
        std::cerr << GetShaderBuildLog() << std::endl;
        throw std::runtime_error("Shader compilation error");
    }
}

OpenGLCanvas::~OpenGLCanvas() {
    glDeleteVertexArrays(1, &VAO_);
    glDeleteBuffers(1, &VBO_);

    glDeleteBuffers(1, &EBO_);

    delete openGLContext_;
}

bool OpenGLCanvas::InitializeOpenGLFunctions() {
    GLenum err = glewInit();

    if (GLEW_OK != err) {
        wxLogError("OpenGL GLEW initialization failed: %s", reinterpret_cast<const char *>(glewGetErrorString(err)));
        return false;
    }

    wxLogDebug("Status: Using GLEW %s", reinterpret_cast<const char *>(glewGetString(GLEW_VERSION)));

    return true;
}

bool OpenGLCanvas::InitializeOpenGL() {
    if (!openGLContext_) {
        return false;
    }

    SetCurrent(*openGLContext_);

    if (!InitializeOpenGLFunctions()) {
        wxMessageBox("Error: Could not initialize OpenGL function pointers.", "OpenGL initialization error",
                     wxOK | wxICON_INFORMATION, this);
        return false;
    }

    wxLogDebug("OpenGL version: %s", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    wxLogDebug("OpenGL vendor: %s", reinterpret_cast<const char *>(glGetString(GL_VENDOR)));

    // Setup GL debug callback if available (KHR_debug)
    if (GLEW_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

        glDebugMessageCallback(GLDebugCallbackFunc, this);

        // Enable all messages (you can filter with glDebugMessageControl)
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        wxLogDebug("KHR_debug is available: GL debug output enabled");
    } else {
        wxLogDebug("KHR_debug not available; GL debug output disabled");
    }

    CompileShaderProgram();

    // If ways were provided before GL initialization, upload them now.
    UpdateBuffersFromRoutes();

    isOpenGLInitialized_ = true;
    openGLInitializationTime_ = std::chrono::high_resolution_clock::now();
    // initialize FPS timer state
    lastFpsUpdateTime_ = std::chrono::high_resolution_clock::now();
    framesSinceLastFps_ = 0;

    wxCommandEvent evt(wxEVT_OPENGL_INITIALIZED);
    evt.SetEventObject(this);
    ProcessWindowEvent(evt);

    return true;
}

void OpenGLCanvas::OnPaint(wxPaintEvent &WXUNUSED(event)) {
    wxPaintDC dc(this);

    if (!isOpenGLInitialized_) {
        return;
    }

    SetCurrent(*openGLContext_);

    float clearColor = 0.87f;
    glClearColor(clearColor, clearColor, clearColor, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (shaderProgram_.shaderProgram_.has_value()) {
        glUseProgram(shaderProgram_.shaderProgram_.value());

        // upload bounds uniform: (minLon, minLat, lonRange, latRange)
        double minLon = bounds_.left();
        double maxLon = bounds_.right();
        double minLat = bounds_.bottom();
        double maxLat = bounds_.top();
        double lonRange = (maxLon - minLon);
        double latRange = (maxLat - minLat);
        // Avoid zero ranges
        if (lonRange == 0.0)
            lonRange = 1.0;
        if (latRange == 0.0)
            latRange = 1.0;
        GLint loc = glGetUniformLocation(shaderProgram_.shaderProgram_.value(), "uBounds");
        if (loc >= 0) {
            glUniform4f(loc, static_cast<float>(minLon), static_cast<float>(minLat), static_cast<float>(lonRange),
                        static_cast<float>(latRange));
        }

        glBindVertexArray(VAO_);
        if (!drawCommands_.empty()) {
            for (const auto &cmd : drawCommands_) {
                GLsizei count = cmd.first;
                const void *offset = reinterpret_cast<const void *>(cmd.second);
                glDrawElements(GL_LINE_STRIP_ADJACENCY, count, GL_UNSIGNED_INT, offset);
            }
        } else {
            glDrawElements(GL_LINE_STRIP_ADJACENCY, elementCount_, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0); // Unbind VAO_ for safety
    }
    SwapBuffers();

    // Update FPS counters and draw overlay text
    ++framesSinceLastFps_;
    auto now = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsUpdateTime_);
    if (dur.count() >= 250) { // update FPS every 250ms for smoother display
        float seconds = dur.count() / 1000.0f;
        if (seconds > 0.0f) {
            fps_ = static_cast<float>(framesSinceLastFps_) / seconds;
        }
        framesSinceLastFps_ = 0;
        lastFpsUpdateTime_ = now;
    }

    // Draw FPS using wx overlay drawing so it's on top of GL content.
    // Use a small margin from the top-left corner.
    wxClientDC overlayDc(this);
    wxFont font = overlayDc.GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    font.SetPointSize(10);
    overlayDc.SetFont(font);
    overlayDc.SetTextForeground(*wxBLACK);
    std::ostringstream ss;
    ss.setf(std::ios::fixed);
    ss.precision(1);
    ss << "FPS: " << fps_;
    const std::string fpsText = ss.str();
    const int margin = 8;
    overlayDc.DrawText(fpsText, margin, margin);
}

void OpenGLCanvas::OnSize(wxSizeEvent &event) {
    bool firstApperance = IsShownOnScreen() && !isOpenGLInitialized_;

    if (firstApperance) {
        InitializeOpenGL();
    }

    if (isOpenGLInitialized_) {
        SetCurrent(*openGLContext_);

        auto viewPortSize = event.GetSize() * GetContentScaleFactor();
        glViewport(0, 0, viewPortSize.x, viewPortSize.y);
    }

    event.Skip();
}

void OpenGLCanvas::OnTimer(wxTimerEvent &WXUNUSED(event)) {
    if (isOpenGLInitialized_) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - openGLInitializationTime_);
        elapsedSeconds_ = duration.count() / 1000.0f;
        Refresh(false);
    }
}

void OpenGLCanvas::OnLeftDown(wxMouseEvent &event) {
    isDragging_ = true;
    lastMousePos_ = event.GetPosition();
    CaptureMouse();
}

void OpenGLCanvas::OnLeftUp(wxMouseEvent &event) {
    if (isDragging_) {
        isDragging_ = false;
        if (HasCapture())
            ReleaseMouse();
    }
}

void OpenGLCanvas::OnMouseMotion(wxMouseEvent &event) {
    if (!isDragging_)
        return;

    if (!event.Dragging() || !event.LeftIsDown())
        return;

    // compute pixel delta (use logical coordinates then account for content
    // scale)
    wxPoint pos = event.GetPosition();
    // use content scale factor to match viewport used for GL
    auto scale = GetContentScaleFactor();
    wxPoint posScaled = pos * scale;
    wxPoint lastScaled = lastMousePos_ * scale;

    int dx = posScaled.x - lastScaled.x;
    int dy = posScaled.y - lastScaled.y;

    auto viewPortSize = GetClientSize() * scale;
    if (viewPortSize.x <= 0 || viewPortSize.y <= 0) {
        lastMousePos_ = pos;
        return;
    }

    double minLon = bounds_.left();
    double maxLon = bounds_.right();
    double minLat = bounds_.bottom();
    double maxLat = bounds_.top();

    double lonRange = (maxLon - minLon);
    double latRange = (maxLat - minLat);
    if (lonRange == 0.0)
        lonRange = 1.0;
    if (latRange == 0.0)
        latRange = 1.0;

    double w = static_cast<double>(viewPortSize.x);
    double h = static_cast<double>(viewPortSize.y);

    // map pixel delta to world delta: lonOffset = -(dx/w)*lonRange
    // and latOffset = (dy/h)*latRange (note y pixel increases downwards)
    double lonOffset = -static_cast<double>(dx) / w * lonRange;
    double latOffset = static_cast<double>(dy) / h * latRange;

    double newMinLon = minLon + lonOffset;
    double newMaxLon = maxLon + lonOffset;
    double newMinLat = minLat + latOffset;
    double newMaxLat = maxLat + latOffset;

    bounds_ = osmium::Box({newMinLon, newMinLat}, {newMaxLon, newMaxLat});

    lastMousePos_ = pos;

    // just request redraw; shader reads `bounds_` uniform during paint
    Refresh(false);
}

void OpenGLCanvas::OnMouseWheel(wxMouseEvent &event) {
    if (event.GetTimestamp() == 0 || event.GetTimestamp() == prevEventTimestamp_) {
        // Ignore synthetic events with duplicate timestamps (e.g. generated on
        // Windows when Alt key is pressed).
        return;
    }
    prevEventTimestamp_ = event.GetTimestamp();

    // Use wheel rotation to compute zoom steps
    const int rotation = event.GetWheelRotation();
    const int delta = event.GetWheelDelta();
    if (delta == 0 || rotation == 0)
        return;

    const int steps = (rotation / delta);

    // scale per step (<1 zooms in, >1 zooms out when steps negative)
    const double stepScale = 0.9;
    const double scale = std::pow(stepScale, steps);

    Zoom(scale, event.GetPosition());
}

void OpenGLCanvas::OnZoomGesture(wxZoomGestureEvent &event) {
    if (event.IsGestureStart()) {
        lastZoomFactor_ = 1.0;
    }

    double currentZoomFactor = event.GetZoomFactor();
    // Viewport range should scale inversely with magnification
    double scale = 1.0 / (currentZoomFactor / lastZoomFactor_);
    lastZoomFactor_ = currentZoomFactor;

    Zoom(scale, event.GetPosition());
}

void OpenGLCanvas::Zoom(double scale, const wxPoint &mousePos) {
    // current bounds
    double minLon = bounds_.left();
    double maxLon = bounds_.right();
    double minLat = bounds_.bottom();
    double maxLat = bounds_.top();

    double lonRange = (maxLon - minLon);
    double latRange = (maxLat - minLat);
    if (lonRange == 0.0 || latRange == 0.0)
        return;

    // Map mouse position to [0,1] in lon/lat space. Need to account for
    // content scale factor used when setting the viewport.
    auto viewPortSize = GetClientSize() * GetContentScaleFactor();
    if (viewPortSize.x <= 0 || viewPortSize.y <= 0)
        return;

    auto pos = mousePos * GetContentScaleFactor();
    double mx = static_cast<double>(pos.x);
    double my = static_cast<double>(pos.y);
    double w = static_cast<double>(viewPortSize.x);
    double h = static_cast<double>(viewPortSize.y);

    double xNorm = mx / w;
    double yNorm = 1.0 - (my / h); // invert Y (wx origin is top-left)

    // clamp
    xNorm = std::min(std::max(xNorm, 0.0), 1.0);
    yNorm = std::min(std::max(yNorm, 0.0), 1.0);

    double centerLon = minLon + xNorm * lonRange;
    double centerLat = minLat + yNorm * latRange;

    double newLonRange = lonRange * scale;
    double newLatRange = latRange * scale;

    const double kMinRange = 1e-12;
    if (newLonRange < kMinRange || newLatRange < kMinRange)
        return;

    // Adjust new min/max to keep mouse position fixed in lon/lat space
    double newMinLon = centerLon - newLonRange * xNorm;
    double newMaxLon = centerLon + newLonRange * (1.0 - xNorm);
    double newMinLat = centerLat - newLatRange * yNorm;
    double newMaxLat = centerLat + newLatRange * (1.0 - yNorm);

    bounds_ = osmium::Box({newMinLon, newMinLat}, {newMaxLon, newMaxLat});

    Refresh(false);
}