#include "model3d.h"
#include "model_io.h"
#include "mesh_processor.h"
#include <iostream>

Model3D::Model3D() 
    : m_io(std::make_unique<ModelIO>(this)),
      m_meshProcessor(std::make_unique<MeshProcessor>(this)) {
}

Model3D::~Model3D() {
    // 智能指针会自动管理组件的生命周期
}

bool Model3D::loadModel(const std::string& filePath) {
    return m_io->loadModel(filePath);
}

void Model3D::getBoundingBox(Vec3& min, Vec3& max) const {
    min = m_boundingBoxMin;
    max = m_boundingBoxMax;
}

void Model3D::printModelInfo() const {
    std::cout << "模型类型: ";
    switch (m_modelType) {
        case ModelType::STL_ASCII: std::cout << "STL ASCII"; break;
        case ModelType::STL_BINARY: std::cout << "STL Binary"; break;
        case ModelType::OBJ: std::cout << "OBJ"; break;
        default: std::cout << "未知"; break;
    }
    std::cout << std::endl;

    std::cout << "包含网格: " << m_meshes.size() << std::endl;
    
    size_t totalVertices = 0;
    size_t totalTriangles = 0;
    for (const auto& mesh : m_meshes) {
        totalVertices += mesh.getVertexCount();
        totalTriangles += mesh.getTriangleCount();
    }
    
    std::cout << "总顶点数: " << totalVertices << std::endl;
    std::cout << "总三角形数: " << totalTriangles << std::endl;
    
    std::cout << "包围盒最小点: (" << m_boundingBoxMin.x << ", " 
              << m_boundingBoxMin.y << ", " << m_boundingBoxMin.z << ")" << std::endl;
    std::cout << "包围盒最大点: (" << m_boundingBoxMax.x << ", " 
              << m_boundingBoxMax.y << ", " << m_boundingBoxMax.z << ")" << std::endl;
}

void Model3D::printMeshStatistics(const std::vector<Mesh>& meshes) const {
    std::cout << "模型类型: ";
    switch (m_modelType) {
        case ModelType::STL_ASCII: std::cout << "STL ASCII"; break;
        case ModelType::STL_BINARY: std::cout << "STL Binary"; break;
        case ModelType::OBJ: std::cout << "OBJ"; break;
        default: std::cout << "未知"; break;
    }
    std::cout << std::endl;

    std::cout << "包含网格: " << meshes.size() << std::endl;
    
    size_t totalVertices = 0;
    size_t totalTriangles = 0;
    for (const auto& mesh : meshes) {
        totalVertices += mesh.getVertexCount();
        totalTriangles += mesh.getTriangleCount();
    }
    
    std::cout << "总顶点数: " << totalVertices << std::endl;
    std::cout << "总三角形数: " << totalTriangles << std::endl;
    
    std::cout << "包围盒最小点: (" << m_boundingBoxMin.x << ", " 
              << m_boundingBoxMin.y << ", " << m_boundingBoxMin.z << ")" << std::endl;
    std::cout << "包围盒最大点: (" << m_boundingBoxMax.x << ", " 
              << m_boundingBoxMax.y << ", " << m_boundingBoxMax.z << ")" << std::endl;
}

void Model3D::clear() {
    m_meshes.clear();
    m_materials.clear();
    m_modelType = ModelType::UNKNOWN;
    m_boundingBoxMin = Vec3(std::numeric_limits<float>::max());
    m_boundingBoxMax = Vec3(-std::numeric_limits<float>::max());
    m_directory.clear();
}

bool Model3D::exportToSTL(const std::string& filePath, bool binary, bool mergeMeshes) const {
    return m_io->exportToSTL(filePath,m_meshes,binary, mergeMeshes);
}

bool Model3D::exportToSTL(const std::string& filePath, const std::vector<Mesh>& meshes, bool binary, bool mergeMeshes) const {
    return m_io->exportToSTL(filePath, meshes, binary, mergeMeshes);
}

bool Model3D::exportToOBJ(const std::string& filePath) const {
    return m_io->exportToOBJ(filePath, m_meshes);
}

std::vector<Mesh> Model3D::extractSurfaces(float angleThreshold) {
    return m_meshProcessor->extractSurfaces(angleThreshold);
}

std::vector<Mesh> Model3D::extractSurfacesByRegionGrowing(float angleThreshold) {
    return m_meshProcessor->extractSurfacesByRegionGrowing(angleThreshold);
}

Mesh Model3D::findTopSurface() {
    return m_meshProcessor->findTopSurface();
}

void Model3D::optimizeMesh(Mesh& mesh) {
    return m_meshProcessor->optimizeMesh(mesh);
}

void Model3D::calculateNormals(Mesh& mesh) {
    return m_meshProcessor->calculateNormals(mesh);
}
