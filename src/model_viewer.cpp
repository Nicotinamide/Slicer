#include "model_viewer.h"
#include <iostream>

wxBEGIN_EVENT_TABLE(ModelViewer, wxGLCanvas)
    EVT_PAINT(ModelViewer::OnPaint)
    EVT_SIZE(ModelViewer::OnSize)
    EVT_MOUSE_EVENTS(ModelViewer::OnMouseEvent)
wxEND_EVENT_TABLE()

// 构建 Canvas 所需的 GL 属性
static const int attribList[] = {
    WX_GL_RGBA,
    WX_GL_DOUBLEBUFFER,
    WX_GL_DEPTH_SIZE, 16,
    WX_GL_SAMPLES, 4, // 抗锯齿
    0
};


// Helper function to convert aiMatrix4x4 to glm::mat4
// Assimp's matrix is row-major, GLM's matrix is column-major.
inline glm::mat4 assimpToGlmMatrix(const aiMatrix4x4& aiMat) {
    glm::mat4 glmMat;
    // Assimp stores matrix in row-major order, so we read it row by row
    // and fill GLM's column-major matrix.
    // Equivalently, this is transposing the Assimp matrix.
    glmMat[0][0] = aiMat.a1; glmMat[1][0] = aiMat.a2; glmMat[2][0] = aiMat.a3; glmMat[3][0] = aiMat.a4;
    glmMat[0][1] = aiMat.b1; glmMat[1][1] = aiMat.b2; glmMat[2][1] = aiMat.b3; glmMat[3][1] = aiMat.b4;
    glmMat[0][2] = aiMat.c1; glmMat[1][2] = aiMat.c2; glmMat[2][2] = aiMat.c3; glmMat[3][2] = aiMat.c4;
    glmMat[0][3] = aiMat.d1; glmMat[1][3] = aiMat.d2; glmMat[2][3] = aiMat.d3; glmMat[3][3] = aiMat.d4;
    return glmMat;
}


ModelViewer::ModelViewer(wxWindow* parent, wxWindowID id)
    : wxGLCanvas(parent, id, attribList, wxDefaultPosition, wxDefaultSize)
{
    // 创建 OpenGL 上下文
    m_context = new wxGLContext(this);
    
    // 第一次设置当前上下文
    SetCurrent(*m_context);
    
    // 初始化 GLEW
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        // GLEW 初始化失败
        std::cerr << "GLEW 初始化错误: " << glewGetErrorString(err) << std::endl;
    }
}

ModelViewer::~ModelViewer()
{
    // 释放 GL 资源
    for (auto& mesh : m_meshes) {
        glDeleteBuffers(1, &mesh.vbo);
        glDeleteBuffers(1, &mesh.ebo);
        glDeleteVertexArrays(1, &mesh.vao);
    }
    
    delete m_context;
}

bool ModelViewer::loadModel(const aiScene* scene)
{
    // 清理现有数据
    for (auto& mesh : m_meshes) {
        // 确保在GL上下文激活时调用glDelete*
        // 如果这里没有激活的上下文，这些调用可能无效或出错
        // 最好将清理操作也放在能够确保GL上下文有效的地方
        // glDeleteBuffers(1, &mesh.vbo);
        // glDeleteBuffers(1, &mesh.ebo);
        // glDeleteVertexArrays(1, &mesh.vao);
    }
    m_meshes.clear(); // 清理CPU端数据

    if (!scene) {
        std::cerr << "空场景对象!" << std::endl;
        return false;
    }

    // 递归处理场景节点，传入初始的单位变换矩阵
    processAiNode(scene->mRootNode, scene, glm::mat4(1.0f));

    // 初始化 GL 状态
    SetCurrent(*m_context);  // 确保有激活的 GL 上下文
    setupGL();              // 调用设置函数创建必要的 OpenGL 缓冲区

    // 自动调整视图
    float radius = calculateModelRadius();
    if(radius > 0.1f) {
        m_scale = 1.0f / (radius * 1.25f);
    }
    
    // 重置旋转
    m_rotationX = 0.0f;
    m_rotationY = 0.0f;

    Refresh();              // 请求重绘

    return true;
}

// 修改 processAiNode 以接受并传递父节点变换
void ModelViewer::processAiNode(const aiNode* node, const aiScene* scene, const glm::mat4& parentTransform)
{
    // 1. 获取当前节点的局部变换并转换为glm::mat4
    glm::mat4 localTransform = assimpToGlmMatrix(node->mTransformation);

    // 2. 计算当前节点的世界变换
    glm::mat4 currentNodeWorldTransform = parentTransform * localTransform;

    // 3. 处理当前节点的所有网格，传递计算出的世界变换
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        processAiMesh(mesh, scene, currentNodeWorldTransform); // <--- 修改：传递世界变换
    }

    // 4. 递归处理子节点，将当前节点的世界变换作为父变换传递下去
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processAiNode(node->mChildren[i], scene, currentNodeWorldTransform); // <--- 修改：传递累积变换
    }
}

// 修改 processAiMesh 以接受并存储世界变换
void ModelViewer::processAiMesh(const aiMesh* mesh, const aiScene* scene, const glm::mat4& worldTransform)
{
    ModelMesh modelMesh;
    modelMesh.worldTransform = worldTransform; // <--- 新增：存储世界变换

    // 收集顶点数据 (位置+法线)
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        // 位置
        modelMesh.vertices.push_back(mesh->mVertices[i].x);
        modelMesh.vertices.push_back(mesh->mVertices[i].y);
        modelMesh.vertices.push_back(mesh->mVertices[i].z);

        // 法线 (如果有)
        if (mesh->HasNormals()) {
            modelMesh.vertices.push_back(mesh->mNormals[i].x);
            modelMesh.vertices.push_back(mesh->mNormals[i].y);
            modelMesh.vertices.push_back(mesh->mNormals[i].z);
        } else {
            // 没有法线时使用默认值
            modelMesh.vertices.push_back(0.0f);
            modelMesh.vertices.push_back(0.0f);
            modelMesh.vertices.push_back(1.0f); // 默认向上(Z)或向前，取决于坐标系
        }
        // 你可能还需要处理纹理坐标 (mesh->mTextureCoords[0]) 等其他顶点属性
    }

    // 收集索引数据
    // std::vector<unsigned int> sortedIndices; // 这部分命名可能引起误解，它并没有排序
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        if (face.mNumIndices == 3) { // 确保是三角形 (Assimp的aiProcess_Triangulate可以保证)
            // const unsigned int indices[3] = { // 不需要额外数组
            //     face.mIndices[0], face.mIndices[1], face.mIndices[2]
            // };
            // 你写的 "将三角形索引排序以保持一致方向" 注释处的代码实际并未排序。
            // 如果确实需要排序，可以使用 std::sort。但通常不需要，因为原始顺序定义了面片朝向。
            modelMesh.indices.push_back(face.mIndices[0]);
            modelMesh.indices.push_back(face.mIndices[1]);
            modelMesh.indices.push_back(face.mIndices[2]);
        }
    }
    // modelMesh.indices = std::move(sortedIndices); // 不再需要，直接push_back到modelMesh.indices

    modelMesh.numIndices = modelMesh.indices.size();

    // 注意：VAO, VBO, EBO的创建 (glGenVertexArrays, glGenBuffers, glBindBuffer, glBufferData等)
    // 应该在 `setupGL()` 函数中，或者在 `processAiMesh` 的末尾，
    // 并且需要确保GL上下文是激活的。
    // 这里只准备CPU端数据。

    m_meshes.push_back(std::move(modelMesh));
}

void ModelViewer::setupGL()
{
    // 初始化 OpenGL 状态
    glEnable(GL_DEPTH_TEST);
    // glEnable(GL_CULL_FACE);  // 可以注释掉这行以显示所有面
    // glCullFace(GL_BACK);
    
    // 为每个网格设置 VAO、VBO 和 EBO
    for (auto& mesh : m_meshes) {
        // 创建 VAO
        glGenVertexArrays(1, &mesh.vao);
        glBindVertexArray(mesh.vao);
        
        // 创建 VBO
        glGenBuffers(1, &mesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float), 
                    mesh.vertices.data(), GL_STATIC_DRAW);
                    
        // 创建 EBO
        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int),
                    mesh.indices.data(), GL_STATIC_DRAW);
                    
        // 设置顶点属性
        // 位置属性
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // 法线属性
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 
                             (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
    }
}

void ModelViewer::OnPaint(wxPaintEvent& event)
{
    // 必需的
    wxPaintDC dc(this);
    
    // 设置当前 GL 上下文
    SetCurrent(*m_context);
    
    // 渲染场景
    render();
    
    // 交换缓冲区
    SwapBuffers();
}

void ModelViewer::OnSize(wxSizeEvent& event)
{
    // 必需的
    wxSize size = event.GetSize();
    
    if (m_context) {  // 修改此行，使用 m_context 而不是 GetContext()
        SetCurrent(*m_context);
        glViewport(0, 0, size.x, size.y);
    }
    event.Skip();
}

void ModelViewer::OnMouseEvent(wxMouseEvent& event)
{
    static wxPoint lastPos;
    
    if (event.LeftDown()) {
        lastPos = event.GetPosition();
    }
    else if (event.Dragging() && event.LeftIsDown()) {
        wxPoint pos = event.GetPosition();
        int dx = pos.x - lastPos.x;
        int dy = pos.y - lastPos.y;
        
        m_rotationY += dx * 0.5f;
        m_rotationX += dy * 0.5f;
        
        lastPos = pos;
        Refresh();
    }
    else if (event.GetWheelRotation() != 0) {
        // 调整缩放灵敏度，使用更明显的变化
        m_scale += (event.GetWheelRotation() > 0) ? 0.1f : -0.1f;
        if (m_scale < 0.1f) m_scale = 0.1f;
        
        // 输出调试信息
        std::cout << "滚轮旋转: " << event.GetWheelRotation() 
                  << ", 新缩放值: " << m_scale << std::endl;
        
        // 确保视图更新
        Refresh();
    }
    
    event.Skip();
}

void ModelViewer::render()
{
    // 设置背景色
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 如果没有模型数据则返回
    if (m_meshes.empty())
        return;
    
    // 设置投影矩阵
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    wxSize size = GetSize();
    float aspect = static_cast<float>(size.x) / size.y;
    gluPerspective(45.0f, aspect, 0.1f, 100.0f);
    
    // 设置模型视图矩阵
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef(m_rotationX, 1.0f, 0.0f, 0.0f);
    glRotatef(m_rotationY, 0.0f, 1.0f, 0.0f);
    glScalef(m_scale, m_scale, m_scale);
    
    // 启用光照
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);  // 添加第二光源
    
    // 禁用光照衰减
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.0f);
    
    // 设置第一个光源 (从前方照射)
    GLfloat light0Pos[] = { 0.0f, 0.0f, 10.0f, 0.0f };  // 方向光
    GLfloat light0Amb[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    GLfloat light0Diff[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light0Amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diff);
    
    // 设置第二个光源 (从后方照射)
    GLfloat light1Pos[] = { 0.0f, 0.0f, -10.0f, 0.0f };  // 方向光
    GLfloat light1Amb[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat light1Diff[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
    glLightfv(GL_LIGHT1, GL_AMBIENT, light1Amb);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1Diff);
    
    // 设置材质属性
    GLfloat matAmb[] = { 0.8f, 0.8f, 0.8f, 1.0f };  // 强环境反射
    GLfloat matDiff[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // 满强度漫反射
    GLfloat matEmission[] = { 0.1f, 0.1f, 0.1f, 1.0f }; // 轻微自发光
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiff);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, matEmission);
    
    // 禁用面剔除，显示所有面
    glDisable(GL_CULL_FACE);
    
    // 绘制所有网格
    for (auto& mesh : m_meshes) {
        glBindVertexArray(mesh.vao);
        
        // 绘制填充面
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
        
        // 绘制线框 (可选)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_LIGHTING);  // 禁用光照以便线框清晰可见
        glColor3f(0.0f, 0.0f, 0.0f);  // 黑色线框
        glLineWidth(1.0f);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
        glDisable(GL_POLYGON_OFFSET_LINE);
        glEnable(GL_LIGHTING);  // 重新启用光照
    }
    
    // 禁用光源和光照
    glDisable(GL_LIGHT1);
    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHTING);
}

float ModelViewer::calculateModelRadius()
{
    if (m_meshes.empty())
        return 1.0f;
        
    float maxDist = 0.0f;
    // 遍历所有顶点找到最远距离
    for (const auto& mesh : m_meshes) {
        for (size_t i = 0; i < mesh.vertices.size(); i += 6) { // 每6个浮点数为一个顶点(xyz+normal)
            float x = mesh.vertices[i];
            float y = mesh.vertices[i+1];
            float z = mesh.vertices[i+2];
            float dist = sqrt(x*x + y*y + z*z);
            maxDist = std::max(maxDist, dist);
        }
    }
    
    return maxDist > 0.0f ? maxDist : 1.0f;
}