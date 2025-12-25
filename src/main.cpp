// TODO: move the wxWidgets functionality into a separate module
#include <wx/cmdline.h>
#include <wx/file.h>
#include <wx/log.h>
#include <wx/settings.h>
#include <wx/splitter.h>
#include <wx/stc/stc.h>
#include <wx/wx.h>

#include "openglcanvas.h"
#include "osm_loader.h"

constexpr size_t IndentWidth = 4;

class MyApp : public wxApp {
public:
    MyApp() { }
    bool OnInit() wxOVERRIDE;
    void OnInitCmdLine(wxCmdLineParser& parser) wxOVERRIDE;
    bool OnCmdLineParsed(wxCmdLineParser& parser) wxOVERRIDE;

    wxString scriptFilePath_ {};
};

class MyFrame : public wxFrame {
public:
    MyFrame(const wxString& title, const wxString& scriptFilePath);

protected:
    OpenGLCanvas* openGLCanvas { nullptr };
    wxTextCtrl* logTextCtrl { nullptr };

    wxString scriptFilePath_ {};

    OSMLoader osmLoader_;

    void OnOpenGLInitialized(wxCommandEvent& event);

    void BuildShaderProgram();
    void StylizeTextCtrl();

    void OnSize(wxSizeEvent& event);
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    MyFrame* frame = new MyFrame("Hello OpenGL", scriptFilePath_);
    frame->Show(true);

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
        // pass OSM data file to OSMLoader
        wxString osmDataFilePath = parser.GetParam(1);
        OSMLoader osmLoader;
        osmLoader.setFilepath(osmDataFilePath.ToStdString());
        if (!osmLoader.Count()) {
            wxLogError("Failed to load OSM data from file: %s",
                osmDataFilePath);
            return false;
        }
    }

    return true;
}

MyFrame::MyFrame(const wxString& title, const wxString& scriptFilePath)
    : wxFrame(nullptr, wxID_ANY, title)
    , scriptFilePath_(scriptFilePath)
{
    wxGLAttributes vAttrs;
    vAttrs.PlatformDefaults().Defaults().EndList();

    if (wxGLCanvas::IsDisplaySupported(vAttrs)) {
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
    }

    // osmLoader_.setFilepath();
}

void MyFrame::OnOpenGLInitialized(wxCommandEvent& event)
{
    BuildShaderProgram();
}

void MyFrame::BuildShaderProgram()
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
        }
    }
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