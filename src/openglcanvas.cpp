#include "openglcanvas.h"

#include <shaders.h>

wxDEFINE_EVENT(wxEVT_OPENGL_INITIALIZED, wxCommandEvent);

// constexpr auto VertexShaderSource = R"(#version 330 core

//     layout(location = 0) in vec3 inPosition;

//     void main()
//     {
//         gl_Position = vec4(inPosition, 1.0);
//     }
// )";

// constexpr auto FragmentShaderPrefix = R"(#version 330 core

//     // layout(location = 0) in  fragColor;

//     uniform vec2 iResolution;
//     uniform float iTime;

//     out vec4 FragColor;
// )";

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

    CompileShaderProgram();

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
    GLuint indices[] = {0, 1, 2, 3};

    // store element count so draw code doesn't need a hardcoded value
    elementCount_ = static_cast<GLsizei>(sizeof(indices) / sizeof(indices[0]));

    glGenBuffers(1, &VBO_);
    glGenVertexArrays(1, &VAO_);
    glGenBuffers(1, &EBO_);

    glBindVertexArray(VAO_);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void *)(2 * sizeof(float)));

    // Do not unbind the element array buffer while the VAO is bound.
    // The EBO binding is stored in the VAO state. Unbinding it here would
    // break the VAO's association with the index buffer and cause
    // glDrawElements to read from a null pointer.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

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
        glDrawElements(GL_LINE_STRIP_ADJACENCY, elementCount_, GL_UNSIGNED_INT,
                       0);
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