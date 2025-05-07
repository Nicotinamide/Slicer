#pragma once

#include <GL/glew.h> // 必须最先包含

#include <wx/wx.h>
#include <wx/glcanvas.h> // 这可能会间接包含 gl.h

#include <GL/gl.h>
#include <GL/glu.h>

#include <assimp/scene.h>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> // 用于 glm::make_mat4
#include <glm/gtc/matrix_transform.hpp> // 用于 glm::transpose (虽然我们可能直接构造)


// 模型网格数据
struct ModelMesh {
    unsigned int vao, vbo, ebo;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int numIndices;
    glm::mat4 worldTransform; // 新增：用于存储此网格的世界变换矩阵
};

class ModelViewer : public wxGLCanvas {
public:
    ModelViewer(wxWindow* parent, wxWindowID id = wxID_ANY);
    virtual ~ModelViewer();
    
    // 从 Assimp 加载模型数据
    bool loadModel(const aiScene* scene);
    
private:
    wxGLContext* m_context;
    std::vector<ModelMesh> m_meshes;
    
    // 变换参数
    float m_rotationX = 0.0f;
    float m_rotationY = 0.0f;
    float m_scale = 1.0f;
    
    // 处理事件
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseEvent(wxMouseEvent& event);
    
    // 渲染方法
    void render();
    void setupGL();
    
    // 更新以下两个函数声明，添加缺失的参数
    void processAiNode(const aiNode* node, const aiScene* scene, const glm::mat4& parentTransform);
    void processAiMesh(const aiMesh* mesh, const aiScene* scene, const glm::mat4& worldTransform);

    // 添加计算模型半径的函数
    float calculateModelRadius();
    
    wxDECLARE_EVENT_TABLE();
};