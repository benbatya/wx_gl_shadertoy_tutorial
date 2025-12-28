#include "openglcanvas.h"

#include <shaders.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

wxDEFINE_EVENT(wxEVT_OPENGL_INITIALIZED, wxCommandEvent);

// GL debug callback function used when KHR_debug is available. Logs
// messages (skips notifications) through wxLogError and stderr for
// high-severity messages.
static void GLDebugCallbackFunc(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length,
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

OpenGLCanvas::OpenGLCanvas(wxWindow *parent, const wxGLAttributes &canvasAttrs)
    : wxGLCanvas(parent, canvasAttrs) {
    wxGLContextAttrs ctxAttrs;
    ctxAttrs.PlatformDefaults().CoreProfile().OGLVersion(3, 3).EndList();
    openGLContext = new wxGLContext(this, nullptr, &ctxAttrs);

    if (!openGLContext->IsOK()) {
        wxMessageBox("This sample needs an OpenGL 3.3 capable driver.",
                     "OpenGL version error", wxOK | wxICON_INFORMATION, this);
        delete openGLContext;
        openGLContext = nullptr;
    }

    Bind(wxEVT_PAINT, &OpenGLCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &OpenGLCanvas::OnSize, this);

    timer.SetOwner(this);
    this->Bind(wxEVT_TIMER, &OpenGLCanvas::OnTimer, this);

    constexpr auto FPS = 60.0;
    timer.Start(1000 / FPS);
}

void OpenGLCanvas::SetWays(const OSMLoader::Ways &routes,
                           const osmium::Box &bounds) {
    bounds_ = bounds;
    // Find the longest routes and store only those for testing
    storedWays_.clear();

    constexpr size_t NUM_WAYS = 1;

    std::unordered_map<size_t, std::vector<osmium::object_id_type>> lenIdMap;

    for (const auto &route : routes) {
        auto length = route.second.size();
        lenIdMap[length].push_back(route.first);
    }

    std::vector<size_t> sortedLengths;
    for (const auto &pair : lenIdMap) {
        sortedLengths.push_back(pair.first);
    }
    std::sort(sortedLengths.rbegin(), sortedLengths.rend());

    size_t count = 0;
    for (size_t length : sortedLengths) {
        for (auto routeId : lenIdMap[length]) {
            if (count >= NUM_WAYS)
                break;
            storedWays_[routeId] = routes.at(routeId);
            std::cout << "Selected way ID " << routeId << " with length "
                      << length << std::endl;
            ++count;
        }
    }

    // take first N routes
    // size_t count = 0;
    // for (const auto &route : routes) {
    //     if (count >= NUM_WAYS) {
    //         break;
    //     }
    //     ++count;
    //     storedRoutes_[route.first] = route.second;
    // }

    // Take all ways
    // storedRoutes_ = routes;

    if (isOpenGLInitialized) {
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

    // // Compute bounds
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

    size_t indexOffset = 0;
    for (const auto &entry : storedWays_) {
        const auto &coords = entry.second;
        if (coords.size() < 2)
            continue;

        GLuint base = static_cast<GLuint>(vertices.size() / 5);

        // vertices
        for (const auto &loc : coords) {
            if (!loc.valid())
                continue;
            double lon = loc.lon();
            double lat = loc.lat();
            double deltaLon = lon - minLon;
            double deltaLat = lat - minLat;
            double xNorm = deltaLon / lonRange;
            double yNorm = deltaLat / latRange;
            float x = static_cast<float>(xNorm * 2.0 - 1.0);
            float y = static_cast<float>(yNorm * 2.0 - 1.0);
            // color: simple dark gray for now
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(0.3f);
            vertices.push_back(0.3f);
            vertices.push_back(0.3f);
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
        indices.push_back(
            base + static_cast<GLuint>((vertices.size() / 5) - 1 - base));
        countHere += 1;

        // record draw command (count, byte offset)
        size_t startByteOffset = indexOffset * sizeof(GLuint);
        drawCommands_.emplace_back(static_cast<GLsizei>(countHere),
                                   startByteOffset);
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
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                     vertices.data(), GL_STATIC_DRAW);

    if (EBO_ == 0)
        glGenBuffers(1, &EBO_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
    if (!indices.empty())
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
                     indices.data(), GL_STATIC_DRAW);

    // vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void OpenGLCanvas::CompileShaderProgram() {
    shaderProgram.vertexShaderSource = VertexShader;
    shaderProgram.geometryShaderSource = GeometryShader;
    shaderProgram.fragmentShaderSource = FragmentShader;
    shaderProgram.Build();

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

    delete openGLContext;
}

bool OpenGLCanvas::InitializeOpenGLFunctions() {
    GLenum err = glewInit();

    if (GLEW_OK != err) {
        wxLogError("OpenGL GLEW initialization failed: %s",
                   reinterpret_cast<const char *>(glewGetErrorString(err)));
        return false;
    }

    wxLogDebug("Status: Using GLEW %s",
               reinterpret_cast<const char *>(glewGetString(GLEW_VERSION)));

    return true;
}

bool OpenGLCanvas::InitializeOpenGL() {
    if (!openGLContext) {
        return false;
    }

    SetCurrent(*openGLContext);

    if (!InitializeOpenGLFunctions()) {
        wxMessageBox("Error: Could not initialize OpenGL function pointers.",
                     "OpenGL initialization error", wxOK | wxICON_INFORMATION,
                     this);
        return false;
    }

    wxLogDebug("OpenGL version: %s",
               reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    wxLogDebug("OpenGL vendor: %s",
               reinterpret_cast<const char *>(glGetString(GL_VENDOR)));

    // Setup GL debug callback if available (KHR_debug)
    if (GLEW_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

        glDebugMessageCallback(GLDebugCallbackFunc, this);

        // Enable all messages (you can filter with glDebugMessageControl)
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0,
                              nullptr, GL_TRUE);
        wxLogDebug("KHR_debug is available: GL debug output enabled");
    } else {
        wxLogDebug("KHR_debug not available; GL debug output disabled");
    }

    CompileShaderProgram();

    // If routes were provided before GL initialization, upload them now.
    if (!storedWays_.empty()) {
        UpdateBuffersFromRoutes();
    } else {
        // From
        // https://github.com/JoeyDeVries/LearnOpenGL/tree/master/src/4.advanced_opengl/9.1.geometry_shader_houses
        // set up vertex data (and buffer(s)) and configure vertex attributes
        // ------------------------------------------------------------------
        GLfloat points[] = {
            -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, // bottom-left
            -0.5f, 0.5f,  0.0f, 1.0f, 0.0f, // top-left
            0.5f,  0.5f,  0.0f, 0.0f, 1.0f, // top-right
            0.5f,  -0.5f, 1.0f, 1.0f, 0.0f, // bottom-right
        };
        GLuint indices[] = {0, 0, 1, 2, 3, 3};

        // store element count so draw code doesn't need a hardcoded value
        elementCount_ =
            static_cast<GLsizei>(sizeof(indices) / sizeof(indices[0]));

        // Create and bind VAO first, then create buffers and upload data while
        // the VAO is bound. This keeps the EBO binding stored in the VAO state
        // and consolidates buffer setup into a small, easy-to-read block.
        glGenVertexArrays(1, &VAO_);
        glBindVertexArray(VAO_);

        glGenBuffers(1, &VBO_);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);

        // element buffer (EBO) is part of VAO state while VAO is bound
        glGenBuffers(1, &EBO_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                     GL_STATIC_DRAW);

        // vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              reinterpret_cast<void *>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              reinterpret_cast<void *>(2 * sizeof(float)));

        // Unbind the array buffer (safe — EBO stays bound to VAO). Unbind VAO
        // to avoid accidental state changes elsewhere.
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        // single draw command for fallback geometry
        drawCommands_.clear();
        drawCommands_.emplace_back(elementCount_, 0);
    }

    isOpenGLInitialized = true;
    openGLInitializationTime = std::chrono::high_resolution_clock::now();

    wxCommandEvent evt(wxEVT_OPENGL_INITIALIZED);
    evt.SetEventObject(this);
    ProcessWindowEvent(evt);

    return true;
}

void OpenGLCanvas::OnPaint(wxPaintEvent &WXUNUSED(event)) {
    wxPaintDC dc(this);

    if (!isOpenGLInitialized) {
        return;
    }

    SetCurrent(*openGLContext);

    glClearColor(0.968f, 0.968f, 0.968f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (shaderProgram.shaderProgram.has_value()) {
        glUseProgram(shaderProgram.shaderProgram.value());

        glBindVertexArray(VAO_);
        if (!drawCommands_.empty()) {
            for (const auto &cmd : drawCommands_) {
                GLsizei count = cmd.first;
                const void *offset = reinterpret_cast<const void *>(cmd.second);
                glDrawElements(GL_LINE_STRIP_ADJACENCY, count, GL_UNSIGNED_INT,
                               offset);
            }
        } else {
            glDrawElements(GL_LINE_STRIP_ADJACENCY, elementCount_,
                           GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0); // Unbind VAO_ for safety
    }
    SwapBuffers();
}

void OpenGLCanvas::OnSize(wxSizeEvent &event) {
    bool firstApperance = IsShownOnScreen() && !isOpenGLInitialized;

    if (firstApperance) {
        InitializeOpenGL();
    }

    if (isOpenGLInitialized) {
        SetCurrent(*openGLContext);

        auto viewPortSize = event.GetSize() * GetContentScaleFactor();
        glViewport(0, 0, viewPortSize.x, viewPortSize.y);
    }

    event.Skip();
}

void OpenGLCanvas::OnTimer(wxTimerEvent &WXUNUSED(event)) {
    if (isOpenGLInitialized) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() -
            openGLInitializationTime);
        elapsedSeconds = duration.count() / 1000.0f;
        Refresh(false);
    }
}