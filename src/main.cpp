
#include "openglcanvas.h"
#include "osm_loader.h"

// TODO: move the wxWidgets functionality into a separate module
#include <wx/cmdline.h>
#include <wx/file.h>
#include <wx/fswatcher.h>
#include <wx/log.h>
#include <wx/settings.h>
#include <wx/splitter.h>
#include <wx/stc/stc.h>
#include <wx/wx.h>

#include <memory>

constexpr size_t IndentWidth = 4;

class MyFrame;

class MyApp : public wxApp {
  public:
    MyApp() {}
    bool OnInit() wxOVERRIDE;
    void OnInitCmdLine(wxCmdLineParser &parser) wxOVERRIDE;
    bool OnCmdLineParsed(wxCmdLineParser &parser) wxOVERRIDE;

  protected:
    wxString osmDataFilePath_{};
    MyFrame *frame_{nullptr};
    std::shared_ptr<OSMLoader> osmLoader_{nullptr};
};

class MyFrame : public wxFrame {
  public:
    MyFrame(const wxString &title);
    bool initialize(const std::shared_ptr<OSMLoader> &osmLoader);
    bool BuildShaderProgram();

  protected:
    void OnOpenGLInitialized(wxCommandEvent &event);
    void StylizeTextCtrl();
    void OnSize(wxSizeEvent &event);

    OpenGLCanvas *openGLCanvas{nullptr};

    std::shared_ptr<OSMLoader> osmLoader_{nullptr};
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit() {
    if (!wxApp::OnInit())
        return false;

    osmLoader_ = std::make_shared<OSMLoader>();
    osmLoader_->setFilepath(osmDataFilePath_.ToStdString());

    frame_ = new MyFrame("Hello OpenGL");
    if (!frame_->initialize(osmLoader_)) {
        return false;
    }
    frame_->Show(true);

    return true;
}

void MyApp::OnInitCmdLine(wxCmdLineParser &parser) {
    wxApp::OnInitCmdLine(parser);

    static const wxCmdLineEntryDesc cmdLineDesc[] = {
        {wxCMD_LINE_PARAM, NULL, NULL, "Input OSM datafile",
         wxCMD_LINE_VAL_STRING},
        {wxCMD_LINE_NONE}};

    parser.SetDesc(cmdLineDesc);
}

bool MyApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    if (!wxApp::OnCmdLineParsed(parser))
        return false;

    if (parser.GetParamCount() > 0) {
        osmDataFilePath_ = parser.GetParam(0);
    } else {
        return false;
    }

    return true;
}

MyFrame::MyFrame(const wxString &title) : wxFrame(nullptr, wxID_ANY, title) {}

bool MyFrame::initialize(const std::shared_ptr<OSMLoader> &osmLoader) {
    osmLoader_ = osmLoader;

    wxGLAttributes vAttrs;
    vAttrs.PlatformDefaults().Defaults().EndList();

    if (!wxGLCanvas::IsDisplaySupported(vAttrs)) {
        wxLogError("OpenGL display attributes not supported!");
        return false;
    }

    openGLCanvas = new OpenGLCanvas(this, vAttrs);

    this->Bind(wxEVT_OPENGL_INITIALIZED, &MyFrame::OnOpenGLInitialized, this);

    this->SetSize(FromDIP(wxSize(1200, 600)));
    this->SetMinSize(FromDIP(wxSize(800, 400)));

    this->Bind(wxEVT_SIZE, &MyFrame::OnSize, this);

    auto routes =
        osmLoader_->getRoutes({{-122.50035, 37.84373}, {-122.46780, 37.85918}});
    std::cout << "Loaded " << routes.size() << " routes from OSM data."
              << std::endl;
    // Upload routes into the OpenGL canvas so it can replace the VBO/EBO.
    if (openGLCanvas) {
        openGLCanvas->SetRoutes(routes);
    }

    return true;
}

void MyFrame::OnOpenGLInitialized(wxCommandEvent &event) {}

wxFont GetMonospacedFont(wxFontInfo &&fontInfo) {
    const wxString preferredFonts[] = {"Menlo", "Consolas", "Monaco",
                                       "DejaVu Sans Mono", "Courier New"};

    for (const wxString &fontName : preferredFonts) {
        fontInfo.FaceName(fontName);
        wxFont font(fontInfo);

        if (font.IsOk() && font.IsFixedWidth()) {
            return font;
        }
    }

    fontInfo.Family(wxFONTFAMILY_TELETYPE);
    return wxFont(fontInfo);
}

void MyFrame::OnSize(wxSizeEvent &event) {
    // std::cout << "OnSize event: " << event.GetSize().GetWidth()
    //           << "x" << event.GetSize().GetHeight() << std::endl;
    // a workaround for the OpenGLCanvas not getting the initial size event
    // if contained in wxSplitterWindow
    if (!openGLCanvas->IsOpenGLInitialized() &&
        openGLCanvas->IsShownOnScreen()) {
        openGLCanvas->InitializeOpenGL();

        // we just need one shot for this workaround, so unbind
        this->Unbind(wxEVT_SIZE, &MyFrame::OnSize, this);
    }
    event.Skip();
}