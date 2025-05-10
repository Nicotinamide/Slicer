#pragma once

#include "model3d.h"
#include <vector>

class MeshProcessor {
public:
    MeshProcessor(Model3D* model);
    ~MeshProcessor() = default;
    
    //------------------------------
    // 网格优化功能
    //------------------------------
    
    // 计算网格法线
    void calculateNormals(Mesh& mesh);
    
    // 优化网格(合并重复顶点等)
    void optimizeMesh(Mesh& mesh);
    
    // 合并多个网格
    Mesh mergeMeshes(const std::vector<Mesh>& meshes);
    
    //------------------------------
    // 表面提取与分析功能
    //------------------------------
    
    // 基于法线聚类的表面分割
    std::vector<Mesh> extractSurfaces(float angleThreshold);
    
    // 基于区域生长的表面分割
    std::vector<Mesh> extractSurfacesByRegionGrowing(float angleThreshold);
    
    // 找到顶面
    Mesh findTopSurface(int upAxis = 2);
    
private:
    //------------------------------
    // 辅助函数
    //------------------------------
    
    // DBSCAN聚类实现
    std::vector<int> dbscanClustering(const std::vector<Vec3>& normals, float eps, int minPts);
    
    // 计算两个法线向量之间的距离
    float normalDistance(const Vec3& a, const Vec3& b);
    
    // 找到eps距离内的邻居
    std::vector<int> findNeighbors(const std::vector<Vec3>& normals, int pointIdx, float eps);
    
    // 计算顶面得分
    float calculateNormalScore(const Mesh& submesh, const Vec3& upVector);
    
    // 计算面法线
    Vec3 calculateFaceNormal(const Mesh& mesh, unsigned int faceIndex);
    
    // 计算面的平均高度
    float calculateFaceHeight(const Mesh& mesh, unsigned int faceIndex, int upAxis);
    
    // 指向拥有者模型的指针
    Model3D* m_model;
    
    // 顶面存储
    Mesh m_topSurface;
};
