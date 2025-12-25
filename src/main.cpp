
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
    MyApp() { }
    bool OnInit() wxOVERRIDE;
    void OnInitCmdLine(wxCmdLineParser& parser) wxOVERRIDE;
    bool OnCmdLineParsed(wxCmdLineParser& parser) wxOVERRIDE;
    void OnEventLoopEnter(wxEventLoopBase* loop) wxOVERRIDE;
    void OnFileSystemEvent(wxFileSystemWatcherEvent& event);

protected:
    wxString scriptFilePath_ {};
    wxString osmDataFilePath_ {};
    std::unique_ptr<wxFileSystemWatcher> fileWatcher_ { nullptr };
    MyFrame* frame_ { nullptr };
    std::shared_ptr<OSMLoader> osmLoader_ { nullptr };
};

class MyFrame : public wxFrame {
public:
    MyFrame(const wxString& title);
    bool initialize(const wxString& scriptFilePath, const std::shared_ptr<OSMLoader>& osmLoader);
    bool BuildShaderProgram();

protected:
    void OnOpenGLInitialized(wxCommandEvent& event);
    void StylizeTextCtrl();
    void OnSize(wxSizeEvent& event);

    OpenGLCanvas* openGLCanvas { nullptr };
    wxTextCtrl* logTextCtrl { nullptr };

    wxString scriptFilePath_ {};
    std::shared_ptr<OSMLoader> osmLoader_ { nullptr };
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    osmLoader_ = std::make_shared<OSMLoader>();
    osmLoader_->setFilepath(osmDataFilePath_.ToStdString());
    if (!osmLoader_->Count()) {
        wxLogError("Failed to load OSM data from file: %s",
            osmDataFilePath_);
        return false;
    }

    frame_ = new MyFrame("Hello OpenGL");
    if (!frame_->initialize(scriptFilePath_, osmLoader_)) {
        return false;
    }
    frame_->Show(true);

    return true;
}

void MyApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    wxApp::OnInitCmdLine(parser);

    static const wxCmdLineEntryDesc cmdLineDesc[] = {
        { wxCMD_LINE_PARAM, NULL, NULL, "Input script file to watch", wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_PARAM, NULL, NULL, "Input OSM datafile", wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_NONE }
    };

    parser.SetDesc(cmdLineDesc);
}

bool MyApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    if (!wxApp::OnCmdLineParsed(parser))
        return false;

    if (parser.GetParamCount() > 0) {
        scriptFilePath_ = parser.GetParam(0);
    } else {
        return false;
    }

    if (parser.GetParamCount() > 1) {
        osmDataFilePath_ = parser.GetParam(1);
    }

    return true;
}

void MyApp::OnEventLoopEnter(wxEventLoopBase* loop)
{
    wxApp::OnEventLoopEnter(loop);
    std::cout << "Event loop entered: " << loop << std::endl;

    if (!loop) {
        fileWatcher_.reset();
        return;
    }

    if (scriptFilePath_.IsEmpty()) {
        return;
    }

    fileWatcher_ = std::make_unique<wxFileSystemWatcher>();
    Bind(wxEVT_FSWATCHER, &MyApp::OnFileSystemEvent, this);
    fileWatcher_->SetOwner(this);
    fileWatcher_->Add(scriptFilePath_, wxFSW_EVENT_MODIFY);
}

void MyApp::OnFileSystemEvent(wxFileSystemWatcherEvent& event)
{
    wxString msg;
    switch (event.GetChangeType()) {
    case wxFSW_EVENT_MODIFY:
        msg.Printf("File modified: %s", event.GetPath().GetFullPath());
        frame_->BuildShaderProgram();
        break;
    case wxFSW_EVENT_CREATE:
        msg.Printf("File created: %s", event.GetPath().GetFullPath());
        break;
    case wxFSW_EVENT_DELETE:
        msg.Printf("File deleted: %s", event.GetPath().GetFullPath());
        // scriptFilePath_.clear();
        break;
        // ... handle other event types
    }
    wxLogMessage(msg); // Log the event for the user
}

MyFrame::MyFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title)
{
}

bool MyFrame::initialize(const wxString& scriptFilePath, const std::shared_ptr<OSMLoader>& osmLoader)
{
    scriptFilePath_ = scriptFilePath;
    osmLoader_ = osmLoader;

    wxGLAttributes vAttrs;
    vAttrs.PlatformDefaults().Defaults().EndList();

    if (!wxGLCanvas::IsDisplaySupported(vAttrs)) {
        wxLogError("OpenGL display attributes not supported!");
        return false;
    }

    wxSplitterWindow* mainSplitter = new wxSplitterWindow(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);

    logTextCtrl = new wxTextCtrl(mainSplitter, wxID_ANY, wxEmptyString, wxDefaultPosition,
        wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

    openGLCanvas = new OpenGLCanvas(mainSplitter, vAttrs);

    this->Bind(wxEVT_OPENGL_INITIALIZED, &MyFrame::OnOpenGLInitialized,
        this);

    mainSplitter->SetSashGravity(1.0);
    mainSplitter->SetMinimumPaneSize(FromDIP(10));
    mainSplitter->SplitHorizontally(openGLCanvas, logTextCtrl, -FromDIP(20));

    this->SetSize(FromDIP(wxSize(1200, 600)));
    this->SetMinSize(FromDIP(wxSize(800, 400)));

    this->Bind(wxEVT_SIZE, &MyFrame::OnSize, this);

    auto routes = osmLoader_->getRoutes({ { -122.46780, 37.84373 }, { -122.50035, 37.85918 } });
    std::cout << "Loaded " << routes.size() << " routes from OSM data." << std::endl;

    return true;
}

void MyFrame::OnOpenGLInitialized(wxCommandEvent& event)
{
    BuildShaderProgram();
}

bool MyFrame::BuildShaderProgram()
{
    // load shader from file
    assert(!scriptFilePath_.IsEmpty());
    wxFile file(scriptFilePath_);
    if (file.IsOpened()) {
        wxString fileContent;
        file.ReadAll(&fileContent);
        openGLCanvas->CompileCustomFragmentShader(
            fileContent.ToStdString());
        // logTextCtrl->SetValue(openGLCanvas->GetShaderBuildLog());

        if (openGLCanvas->GetShaderBuildLog().empty()) {
            logTextCtrl->SetValue("Shader compiled successfully.");
        } else {
            std::cerr << "Shader failed to compile." << std::endl;
            // std::cerr << openGLCanvas->GetShaderBuildLog() << std::endl;
            logTextCtrl->SetValue("Shader failed to compile.\n" + openGLCanvas->GetShaderBuildLog());
            return false;
        }
    }

    return true;
}

wxFont GetMonospacedFont(wxFontInfo&& fontInfo)
{
    const wxString preferredFonts[] = { "Menlo", "Consolas", "Monaco",
        "DejaVu Sans Mono", "Courier New" };

    for (const wxString& fontName : preferredFonts) {
        fontInfo.FaceName(fontName);
        wxFont font(fontInfo);

        if (font.IsOk() && font.IsFixedWidth()) {
            return font;
        }
    }

    fontInfo.Family(wxFONTFAMILY_TELETYPE);
    return wxFont(fontInfo);
}

void MyFrame::OnSize(wxSizeEvent& event)
{
    // std::cout << "OnSize event: " << event.GetSize().GetWidth()
    //           << "x" << event.GetSize().GetHeight() << std::endl;
    // a workaround for the OpenGLCanvas not getting the initial size event
    // if contained in wxSplitterWindow
    if (!openGLCanvas->IsOpenGLInitialized() && openGLCanvas->IsShownOnScreen()) {
        openGLCanvas->InitializeOpenGL();

        // we just need one shot for this workaround, so unbind
        this->Unbind(wxEVT_SIZE, &MyFrame::OnSize, this);
    }
    event.Skip();
}