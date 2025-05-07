// src/gui_main.cpp
#include <wx/wx.h>
#include <wx/filedlg.h> // 包含文件对话框头文件
#include "model_loader.h" // 包含你的模型加载函数声明
#include "model_viewer.h" // 添加 ModelViewer 头文件
#include <iostream> // 添加必要头文件

// --- wxWidgets Application Class ---
class MyApp : public wxApp
{
public:
    virtual bool OnInit() override;
};

// --- wxWidgets Frame Class (主窗口) ---
class MyFrame : public wxFrame
{
public:
    MyFrame(const wxString& title);

private:
    // 事件处理函数
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnOpenModel(wxCommandEvent& event);

    // 添加模型查看器
    ModelViewer* m_modelViewer;

    wxDECLARE_EVENT_TABLE();
};

// --- 定义新的菜单项 ID ---
enum
{
    ID_OpenModel = wxID_HIGHEST + 1 // 自定义菜单项ID
    // 你可以继续添加更多自定义ID
};

// --- 事件表定义 ---
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(wxID_EXIT, MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
    EVT_MENU(ID_OpenModel, MyFrame::OnOpenModel) // 绑定新的菜单事件
wxEND_EVENT_TABLE()

// wxIMPLEMENT_APP(MyApp); // 注释掉这一行

// --- MyApp 方法实现 ---
bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    MyFrame *frame = new MyFrame("My Slicer wxWidgets GUI");
    frame->Show(true);
    return true;
}

// --- MyFrame 方法实现 ---
MyFrame::MyFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(800, 600)) // 窗口稍大些
{
    // --- 创建菜单栏 ---
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(ID_OpenModel, "&Open Model...\tCtrl-O", "Open a 3D model file"); // 添加“打开模型”菜单项
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);

    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);

    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");

    SetMenuBar(menuBar);

    // --- 创建状态栏 ---
    CreateStatusBar();
    SetStatusText("Welcome to My Slicer!");

    // --- 创建主面板和布局 ---
    wxPanel *panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    // 创建模型查看器
    m_modelViewer = new ModelViewer(panel);

    // 添加到布局
    mainSizer->Add(m_modelViewer, 1, wxEXPAND | wxALL, 5);

    panel->SetSizer(mainSizer);
    Layout();
}

// --- MyFrame 事件处理函数实现 ---
void MyFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox(wxString::Format("This is a wxWidgets Slicer Application example.\n"
                                  "Using wxWidgets version: %s\n"
                                  "Current time: %s",
                                  wxVERSION_STRING, wxDateTime::Now().Format()),
                 "About My Slicer", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnOpenModel(wxCommandEvent& event)
{
    // 定义文件对话框支持的文件类型通配符
    // 你可以根据 Assimp 支持的和你希望用户加载的格式来调整这个列表
    wxString wildcard =
        "All supported formats (*.3ds;*.fbx;*.dae;*.obj;*.ply;*.stl;*.gltf;*.glb)|*.3ds;*.fbx;*.dae;*.obj;*.ply;*.stl;*.gltf;*.glb|"
        "STL Files (*.stl)|*.stl|"
        "OBJ Files (*.obj)|*.obj|"
        "FBX Files (*.fbx)|*.fbx|"
        "Collada Files (*.dae)|*.dae|"
        "GLTF/GLB Files (*.gltf;*.glb)|*.gltf;*.glb|"
        "PLY Files (*.ply)|*.ply|"
        "All files (*.*)|*.*";

    wxFileDialog openFileDialog(this, _("Open 3D Model file"), "", "",
                               wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
    {
        SetStatusText("Model loading cancelled.");
        return; // 用户取消了对话框
    }

    // 获取用户选择的文件路径
    wxString filePath = openFileDialog.GetPath();
    std::string stdFilePath = std::string(filePath.ToUTF8()); // 转换为 std::string

    SetStatusText(wxString::Format("Loading model: %s", filePath));

    // 调用 Assimp 加载函数
    const aiScene* scene = loadModelWithAssimp(stdFilePath);
    if (scene)
    {
        // 加载成功
        wxMessageBox(wxString::Format("Successfully loaded model:\n%s", filePath),
                     "Model Loaded", wxOK | wxICON_INFORMATION);
        SetStatusText(wxString::Format("Model loaded: %s", filePath));

        // 将模型数据传递给模型查看器
        m_modelViewer->loadModel(scene);
    }
    else
    {
        // 加载失败 (loadModelWithAssimp 函数内部应该已经打印了更详细的 Assimp错误信息到控制台)
        wxMessageBox(wxString::Format("Failed to load model:\n%s\n\nSee console for Assimp error details.", filePath),
                     "Model Load Error", wxOK | wxICON_ERROR);
        SetStatusText(wxString::Format("Failed to load model: %s", filePath));
    }
}

// 在文件末尾添加标准 main 函数
int main(int argc, char *argv[])
{
    // 可以直接输出到控制台
    std::cout << "=====================================" << std::endl;
    std::cout << "My Slicer Application - Debug Console" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    // 设置 wxWidgets 应用程序实例
    wxApp::SetInstance(new MyApp());
    
    // 手动调用 wxWidgets 入口点
    return wxEntry(argc, argv);
}