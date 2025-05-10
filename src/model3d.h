#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <limits>
#include <cmath> // 用于数学函数
#include <memory> // 用于智能指针

// 自定义 2D 向量结构，替代 glm::vec2
struct Vec2 {
    float x, y;

    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    // 基本操作符
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(float scalar) const { return Vec2(x / scalar, y / scalar); }
};

// 自定义 3D 向量结构，替代 glm::vec3
struct Vec3 {
    union {
        struct { float x, y, z; };     // 坐标访问
        struct { float r, g, b; };     // 颜色访问
    };

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    // 单值构造函数
    explicit Vec3(float value) : x(value), y(value), z(value) {}

    // 基本操作符
    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
    Vec3 operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    
    // 赋值操作符
    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    Vec3& operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }
    
    // 点乘
    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    
    // 叉乘
    Vec3 cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
    
    // 长度
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float squared_length() const { return x * x + y * y + z * z; }
    
    // 规范化
    Vec3 normalize() const {
        float len = length();
        if (len > 0) {
            return *this / len;
        }
        return *this;
    }
    
    // 距离计算
    static float distance(const Vec3& a, const Vec3& b) {
        Vec3 diff = a - b;
        return diff.length();
    }
};

// 模型文件类型
enum class ModelType {
    UNKNOWN,
    STL_ASCII,
    STL_BINARY,
    OBJ
};

// 表示顶点的结构
struct Vertex {
    Vec3 position;  // 位置
    Vec3 normal;    // 法线
    Vec2 texCoord;  // 纹理坐标
    Vec3 color;     // 颜色
};

// 表示三角形的结构
struct Triangle {
    std::array<int, 3> indices;  // 顶点索引
    Vec3 normal;                 // 面法线
};

// 表示材质的结构
struct Material {
    std::string name;            // 材质名称
    Vec3 ambient;                // 环境光颜色
    Vec3 diffuse;                // 漫反射颜色
    Vec3 specular;               // 镜面反射颜色
    float shininess = 1.0f;      // 光泽度
    std::string diffuseMap;      // 漫反射贴图
    std::string normalMap;       // 法线贴图
};

// 表示网格的结构
struct Mesh {
    std::string name;                 // 网格名称
    std::vector<Vertex> vertices;     // 顶点数组
    std::vector<Triangle> triangles;  // 三角形数组
    std::vector<unsigned int> indices;// 索引数组(展平的三角形索引)
    Material material;                // 材质
    Vec3 center;                      // 中心点
    
    // 获取三角形数量
    size_t getTriangleCount() const { return triangles.size(); }
    
    // 获取顶点数量
    size_t getVertexCount() const { return vertices.size(); }
};

// 计算三角形法线的辅助函数 - 更健壮版本
inline Vec3 calculateTriangleNormal(const Vec3& v0, const Vec3& v1, const Vec3& v2) {
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 normal = edge1.cross(edge2);
    
    // 检查是否为退化三角形
    float len = normal.length();
    if (len < std::numeric_limits<float>::epsilon()) {
        // 返回一个默认的向上法线，避免未定义行为
        return Vec3(0.0f, 0.0f, 1.0f);
    }
    
    // 安全归一化
    return normal / len;
}

// 前向声明组件类
class ModelIO;
class MeshProcessor;

// 3D模型处理的主类
class Model3D {
public:
    Model3D();
    ~Model3D();
    
    // 获取模型网格数据
    const std::vector<Mesh>& getMeshes() const { return m_meshes; }
    std::vector<Mesh>& getMeshes() { return m_meshes; }
    
    // 获取模型类型
    ModelType getModelType() const { return m_modelType; }
    
    // 获取包围盒
    void getBoundingBox(Vec3& min, Vec3& max) const;
    
    // 获取模型统计信息
    void printModelInfo() const;
    // 统计输入模型的信息
    void printMeshStatistics(const std::vector<Mesh>& meshes) const;

    
    // 清除数据
    void clear();
    
    // IO操作
    bool loadModel(const std::string& filePath);
    bool exportToSTL(const std::string& filePath, bool binary = false, bool mergeMeshes = true) const;
    bool exportToOBJ(const std::string& filePath) const;

    bool exportToSTL(const std::string& filePath, const std::vector<Mesh>& meshes, bool binary = false, bool mergeMeshes = true) const;


    // 表面处理
    std::vector<Mesh> extractSurfaces(float angleThreshold);
    std::vector<Mesh> extractSurfacesByRegionGrowing(float angleThreshold);
    Mesh findTopSurface();
    
    // 网格优化
    void optimizeMesh(Mesh& mesh);
    void calculateNormals(Mesh& mesh);

private:
    // 模型数据
    std::vector<Mesh> m_meshes;
    ModelType m_modelType = ModelType::UNKNOWN;
    Vec3 m_boundingBoxMin = Vec3(std::numeric_limits<float>::max());
    Vec3 m_boundingBoxMax = Vec3(-std::numeric_limits<float>::max());
    
    // 组件实例 - 使用智能指针管理生命周期
    std::unique_ptr<ModelIO> m_io;
    std::unique_ptr<MeshProcessor> m_meshProcessor;
    
    // 用于OBJ格式的材质映射
    std::unordered_map<std::string, Material> m_materials;
    
    // 记录文件目录(用于查找关联文件如MTL)
    std::string m_directory;
    
    // 允许组件访问Model3D的私有成员
    friend class ModelIO;
    friend class MeshProcessor;
};