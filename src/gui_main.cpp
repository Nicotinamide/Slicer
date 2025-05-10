// src/gui_main.cpp
#include "gui_main.h"

// --- 事件表定义 ---
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(wxID_EXIT, MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
    EVT_MENU(ID_OpenModel, MyFrame::OnOpenModel)
    EVT_BUTTON(ID_AnalyzeTopSurface, MyFrame::OnAnalyzeTopSurface)
    EVT_TOGGLEBUTTON(ID_ShowTopSurfaceOnly, MyFrame::OnToggleTopSurface)
    EVT_BUTTON(ID_ExportTopSurface, MyFrame::OnExportTopSurface)
wxEND_EVENT_TABLE()

// --- MyApp 方法实现 ---
bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    // 设置字符编码
    wxSetlocale(LC_ALL, "zh_CN.UTF-8");

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
    menuFile->Append(ID_OpenModel, "&Open Model...\tCtrl-O", "Open a 3D model file");
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

    // --- 创建主布局 ---
    wxBoxSizer *mainSizer = new wxBoxSizer(wxHORIZONTAL);

    // 创建模型查看器（占左侧大部分区域）
    m_modelViewer = new ModelViewer(this);
    mainSizer->Add(m_modelViewer, 4, wxEXPAND | wxALL, 5);

    // 创建控制面板（右侧）
    m_controlPanel = new wxPanel(this);
    wxBoxSizer *controlSizer = new wxBoxSizer(wxVERTICAL);

    // 添加"分析顶面"按钮
    m_analyzeButton = new wxButton(m_controlPanel, ID_AnalyzeTopSurface, wxString::FromUTF8("分析顶面"));
    m_analyzeButton->Enable(false); // 初始禁用，需要先加载模型
    controlSizer->Add(m_analyzeButton, 0, wxEXPAND | wxALL, 5);

    // 添加"仅显示顶面"切换按钮 - 使用正确的类型
    m_showTopSurfaceButton = new wxToggleButton(m_controlPanel, ID_ShowTopSurfaceOnly, wxString::FromUTF8("仅显示顶面"));
    m_showTopSurfaceButton->Enable(false); // 初始禁用，需要先分析顶面
    controlSizer->Add(m_showTopSurfaceButton, 0, wxEXPAND | wxALL, 5);

    // 添加"导出顶面"按钮
    m_exportButton = new wxButton(m_controlPanel, ID_ExportTopSurface, wxString::FromUTF8("导出顶面"));
    m_exportButton->Enable(false); // 初始禁用，需要先分析顶面
    controlSizer->Add(m_exportButton, 0, wxEXPAND | wxALL, 5);

    // 添加"提取连续表面"按钮
    m_extractSurfacesButton = new wxButton(m_controlPanel, ID_ExtractSurfaces, wxString::FromUTF8("提取连续表面"));
    controlSizer->Add(m_extractSurfacesButton, 0, wxEXPAND | wxALL, 5);

    // 添加"面分离"按钮
    m_separateSurfacesButton = new wxButton(m_controlPanel, ID_SeparateSurfaces, wxString::FromUTF8("面分离"));
    controlSizer->Add(m_separateSurfacesButton, 0, wxEXPAND | wxALL, 5);

    m_controlPanel->SetSizer(controlSizer);
    mainSizer->Add(m_controlPanel, 1, wxEXPAND | wxALL, 5);



    SetSizer(mainSizer);
    Layout();

    // 设置支持中文的字体
    wxFont font(12, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "SimSun");
    m_analyzeButton->SetFont(font);
    m_showTopSurfaceButton->SetFont(font);
    m_exportButton->SetFont(font);
}

// --- MyFrame 事件处理函数实现 ---
void MyFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox(wxString::FromUTF8("这是一个 wxWidgets 切片机应用程序示例。\n") +
                wxString::FromUTF8("使用 wxWidgets 版本: ") + wxVERSION_STRING + wxString::FromUTF8("\n") +
                wxString::FromUTF8("当前时间: ") + wxDateTime::Now().Format(),
                wxString::FromUTF8("关于切片机"), wxOK | wxICON_INFORMATION);
}

void MyFrame::OnOpenModel(wxCommandEvent& event)
{
    // 定义文件对话框支持的文件类型通配符
    wxString wildcard =
        wxString::FromUTF8("所有支持的格式 (*.3ds;*.fbx;*.dae;*.obj;*.ply;*.stl;*.gltf;*.glb)|*.3ds;*.fbx;*.dae;*.obj;*.ply;*.stl;*.gltf;*.glb|"
        "STL 文件 (*.stl)|*.stl|"
        "OBJ 文件 (*.obj)|*.obj|"
        "FBX 文件 (*.fbx)|*.fbx|"
        "Collada 文件 (*.dae)|*.dae|"
        "GLTF/GLB 文件 (*.gltf;*.glb)|*.gltf;*.glb|"
        "PLY 文件 (*.ply)|*.ply|"
        "IFC 文件 (*.ifc)|*.ifc|"
        "所有文件 (*.*)|*.*");

    wxFileDialog openFileDialog(this, wxString::FromUTF8("打开 3D 模型文件"), "", "",
                               wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
    {
        SetStatusText(wxString::FromUTF8("模型加载已取消。"));
        return;
    }

    wxString filePath = openFileDialog.GetPath();
    std::string stdFilePath = std::string(filePath.ToUTF8());

    SetStatusText(wxString::FromUTF8("正在加载模型: ") + filePath);

    const aiScene* scene = loadModelWithAssimp(stdFilePath);
    if (scene)
    {
        wxMessageBox(wxString::FromUTF8("成功加载模型:\n") + filePath,
                     wxString::FromUTF8("模型已加载"), wxOK | wxICON_INFORMATION);
        SetStatusText(wxString::FromUTF8("模型已加载: ") + filePath);

        m_modelViewer->loadModel(scene);
        
        // 启用分析顶面按钮
        m_analyzeButton->Enable(true);
        
        // 禁用顶面相关按钮
        m_showTopSurfaceButton->Enable(false);
        m_showTopSurfaceButton->SetValue(false);
        m_exportButton->Enable(false);
        m_topSurfaceAnalyzed = false;
    }
    else
    {
        wxMessageBox(wxString::FromUTF8("加载模型失败:\n") + filePath + wxString::FromUTF8("\n\n请查看控制台获取 Assimp 错误详情。"),
                     wxString::FromUTF8("模型加载错误"), wxOK | wxICON_ERROR);
        SetStatusText(wxString::FromUTF8("加载模型失败: ") + filePath);
    }
}

// 添加未实现的顶面分析功能函数
void MyFrame::OnAnalyzeTopSurface(wxCommandEvent& event)
{
    // 显示进度信息
    wxBusyCursor wait;
    wxBusyInfo info(wxString::FromUTF8("正在分析顶面，请稍候..."), this);
    
    // 执行顶面分析
    m_modelViewer->analyzeTopSurface();
    
    // 标记分析完成
    m_topSurfaceAnalyzed = true;
    
    // 启用顶面相关按钮
    m_showTopSurfaceButton->Enable(true);
    m_exportButton->Enable(true);
    
    SetStatusText(wxString::FromUTF8("顶面分析完成"));
}

void MyFrame::OnToggleTopSurface(wxCommandEvent& event)
{
    // 根据按钮状态切换显示
    bool showTopSurfaceOnly = m_showTopSurfaceButton->GetValue();
    m_modelViewer->showTopSurfaceOnly(showTopSurfaceOnly);
    
    SetStatusText(showTopSurfaceOnly ? wxString::FromUTF8("仅显示顶面") : wxString::FromUTF8("显示完整模型"));
}

void MyFrame::OnExportTopSurface(wxCommandEvent& event)
{
    wxFileDialog saveFileDialog(this, wxString::FromUTF8("导出顶面"), "", "",
        wxString::FromUTF8("STL文件 (*.stl)|*.stl"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        
    if (saveFileDialog.ShowModal() == wxID_CANCEL)
        return;
        
    // 导出顶面
    std::string stdFilePath = std::string(saveFileDialog.GetPath().ToUTF8());
    if (m_modelViewer->exportTopSurface(stdFilePath)) {
        wxMessageBox(wxString::FromUTF8("顶面已成功导出"), wxString::FromUTF8("导出成功"), wxOK | wxICON_INFORMATION);
        SetStatusText(wxString::FromUTF8("顶面导出成功: ") + saveFileDialog.GetPath());
    } else {
        wxMessageBox(wxString::FromUTF8("导出顶面失败"), wxString::FromUTF8("导出错误"), wxOK | wxICON_ERROR);
        SetStatusText(wxString::FromUTF8("顶面导出失败"));
    }
}

void MyFrame::OnExtractSurfaces(wxCommandEvent& event) {
    m_modelViewer->analyzeSurfaces(10.0f); // 10度角度阈值
    SetStatusText(wxString::FromUTF8("正在提取表面..."));
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