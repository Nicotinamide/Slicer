#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <fstream>
#include <unordered_map>
#include "model.h" // Include the new model header file with custom vector types

// 手动实现的模型读取器
class ModelReader {
public:
    ModelReader() = default;
    ~ModelReader() = default;
    
    // 加载模型文件(STL或OBJ)
    bool loadModel(const std::string& filePath);
    
    // 获取导入的所有网格
    const std::vector<Mesh>& getMeshes() const { return m_meshes; }
    
    // 获取模型类型
    ModelType getModelType() const { return m_modelType; }
      // 获取包围盒
    void getBoundingBox(Vec3& min, Vec3& max) const;
    
    // 获取模型统计信息
    void printModelInfo() const;
      // 导出为STL文件
    bool exportToSTL(const std::string& filePath, bool binary = false, bool mergeMeshes = true) const;
    
    // 导出为OBJ文件
    bool exportToOBJ(const std::string& filePath) const;
    
    // 清除数据
    void clear();
    
private:
    // 检测文件类型
    ModelType detectFileType(const std::string& filePath);
    
    // 读取ASCII STL文件
    bool readSTLAscii(const std::string& filePath);
    
    // 读取二进制STL文件
    bool readSTLBinary(const std::string& filePath);
    
    // 读取OBJ文件
    bool readOBJ(const std::string& filePath);
    
    // 读取MTL材质文件
    bool readMTL(const std::string& filePath);
    
    // 计算法线
    void calculateNormals(Mesh& mesh);
    
    // 优化网格(合并重复顶点等)
    void optimizeMesh(Mesh& mesh);
    
    // 模型类型
    ModelType m_modelType = ModelType::UNKNOWN;
    
    // 模型的网格数据
    std::vector<Mesh> m_meshes;
    
      // 包围盒
    Vec3 m_boundingBoxMin = Vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec3 m_boundingBoxMax = Vec3(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    
    // 用于OBJ格式的材质映射
    std::unordered_map<std::string, Material> m_materials;
    
    // 记录文件目录(用于查找关联文件如MTL)
    std::string m_directory;
};