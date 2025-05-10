#pragma once

#include <vector>
#include "model3d.h"

class SurfaceProcessor {
public:
    SurfaceProcessor(Model3D* model);
    ~SurfaceProcessor() = default;
    
    // 基于法线聚类的表面分割
    std::vector<Mesh> extractSurfaces(float angleThreshold);
    
    // 基于区域生长的表面分割
    std::vector<Mesh> extractSurfacesByRegionGrowing(float angleThreshold);
    
    // 找到顶面
    Mesh findTopSurface(int upAxis = 2);
    
private:
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
