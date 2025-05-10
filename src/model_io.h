#pragma once

#include <string>
#include "model3d.h"

class ModelIO {
public:
    ModelIO(Model3D* model);
    ~ModelIO() = default;
    
    // 加载模型文件(STL或OBJ)
    bool loadModel(const std::string& filePath);
    
    // 导出为STL文件
    bool exportToSTL(const std::string& filePath, const std::vector<Mesh>& meshes, bool binary = false, bool mergeMeshes = true) const;
    // 导出为OBJ文件
    bool exportToOBJ(const std::string& filePath, const std::vector<Mesh>& meshes) const;

    // 导出单个网格到文件
    bool exportMeshToSTL(const std::string& filePath, const Mesh& mesh, bool binary = false) const;
    bool exportMeshToOBJ(const std::string& filePath, const Mesh& mesh) const;

private:
    // 文件类型检测和读取
    ModelType detectFileType(const std::string& filePath);
    bool readSTLAscii(const std::string& filePath);
    bool readSTLBinary(const std::string& filePath);
    bool readOBJ(const std::string& filePath);
    bool readMTL(const std::string& filePath);
    

    // 指向拥有者模型的指针
    Model3D* m_model;
};
