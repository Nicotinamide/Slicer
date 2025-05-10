#pragma once
#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/busyinfo.h>
#include <wx/tglbtn.h>
#include "model_loader.h"
#include "model_viewer.h"
#include <iostream>

class ModelViewer;

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
    // 状态变量
    bool m_topSurfaceAnalyzed = false;

    // 事件处理函数
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnOpenModel(wxCommandEvent& event);
    void OnAnalyzeTopSurface(wxCommandEvent& event);
    void OnToggleTopSurface(wxCommandEvent& event);
    void OnExportTopSurface(wxCommandEvent& event);
    void OnExtractSurfaces(wxCommandEvent& event);


    // UI控件
    ModelViewer* m_modelViewer;
    wxPanel* m_controlPanel;
    wxButton* m_analyzeButton;
    wxButton* m_extractSurfacesButton; // wxToggleButton
    wxToggleButton* m_showTopSurfaceButton;
    wxButton* m_exportButton;
    wxButton* m_separateSurfacesButton; // wxButton


    wxDECLARE_EVENT_TABLE();
};

// --- 定义新的菜单项 ID ---
enum
{
    ID_OpenModel = wxID_HIGHEST + 1,
    ID_AnalyzeTopSurface,
    ID_ShowTopSurfaceOnly,
    ID_ExportTopSurface,
    ID_ExtractSurfaces,
    ID_SeparateSurfaces,
};