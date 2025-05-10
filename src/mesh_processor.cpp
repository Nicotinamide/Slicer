#include "mesh_processor.h"
#include "model3d.h"
#include <iostream>
#include <unordered_map>
#include <queue>
#include <set>
#include <algorithm>
#include <cmath>

MeshProcessor::MeshProcessor(Model3D* model) : m_model(model) {
}

//--------------------------------------------------
// 网格优化功能
//--------------------------------------------------

void MeshProcessor::calculateNormals(Mesh& mesh) {
    // 重置所有顶点法线
    for (auto& vertex : mesh.vertices) {
        vertex.normal = Vec3(0.0f);
    }
    
    // 计算每个三角形的法线并累加到顶点
    for (auto& tri : mesh.triangles) {
        // 获取三角形的三个顶点
        const Vec3& v0 = mesh.vertices[tri.indices[0]].position;
        const Vec3& v1 = mesh.vertices[tri.indices[1]].position;
        const Vec3& v2 = mesh.vertices[tri.indices[2]].position;
        
        // 计算法线
        Vec3 normal = calculateTriangleNormal(v0, v1, v2);
        tri.normal = normal;
        
        // 将法线加到顶点
        for (int i = 0; i < 3; i++) {
            mesh.vertices[tri.indices[i]].normal += normal;
        }
    }
    
    // 归一化顶点法线
    for (auto& vertex : mesh.vertices) {
        if (vertex.normal.squared_length() > 0) {
            vertex.normal = vertex.normal.normalize();
        }
    }
}

void MeshProcessor::optimizeMesh(Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        return;
    }
    
    // 为重复顶点创建映射
    std::unordered_map<int, int> indexMapping;
    std::vector<Vertex> uniqueVertices;
    
    // 设置一个小的epsilon值用于浮点比较
    const float EPSILON = 1e-6f;
    
    // 遍历所有顶点,寻找重复项
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        const Vertex& v = mesh.vertices[i];
        bool found = false;
        
        // 检查是否已存在该顶点
        for (size_t j = 0; j < uniqueVertices.size(); j++) {
            const Vertex& uv = uniqueVertices[j];
            
            // 比较位置 (近似相等)
            if (std::abs(v.position.x - uv.position.x) < EPSILON && 
                std::abs(v.position.y - uv.position.y) < EPSILON && 
                std::abs(v.position.z - uv.position.z) < EPSILON) {
                
                indexMapping[i] = j;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // 添加为新的唯一顶点
            indexMapping[i] = uniqueVertices.size();
            uniqueVertices.push_back(v);
        }
    }
    
    // 使用新的唯一顶点更新三角形索引
    for (auto& tri : mesh.triangles) {
        for (int i = 0; i < 3; i++) {
            tri.indices[i] = indexMapping[tri.indices[i]];
        }
    }
    
    // 更新顶点数组
    mesh.vertices = uniqueVertices;
    
    // 重新计算法线
    calculateNormals(mesh);
    
    // 重新计算中心点
    Vec3 center(0.0f, 0.0f, 0.0f);
    for (const auto& vertex : mesh.vertices) {
        center = center + vertex.position;
    }
    
    if (!mesh.vertices.empty()) {
        center = center / static_cast<float>(mesh.vertices.size());
    }
    mesh.center = center;
    
    std::cout << "网格优化完成: 从 " << indexMapping.size() << " 个顶点减少到 " 
              << uniqueVertices.size() << " 个唯一顶点" << std::endl;
}

Mesh MeshProcessor::mergeMeshes(const std::vector<Mesh>& meshes) {
    if (meshes.empty()) {
        return Mesh();
    }
    
    // 如果只有一个网格,直接返回
    if (meshes.size() == 1) {
        return meshes[0];
    }
    
    Mesh result;
    result.name = "merged_mesh";
    
    // 顶点偏移
    int vertexOffset = 0;
    
    // 遍历所有网格
    for (const auto& mesh : meshes) {
        // 添加顶点
        result.vertices.insert(result.vertices.end(), 
                               mesh.vertices.begin(), 
                               mesh.vertices.end());
        
        // 添加三角形(更新索引)
        for (const auto& tri : mesh.triangles) {
            Triangle newTri;
            newTri.normal = tri.normal;
            
            for (int i = 0; i < 3; i++) {
                newTri.indices[i] = tri.indices[i] + vertexOffset;
            }
            
            result.triangles.push_back(newTri);
        }
        
        // 更新顶点偏移
        vertexOffset += mesh.vertices.size();
    }
    
    // 计算合并后网格的中心点
    Vec3 center(0.0f, 0.0f, 0.0f);
    for (const auto& vertex : result.vertices) {
        center = center + vertex.position;
    }
    
    if (!result.vertices.empty()) {
        center = center / static_cast<float>(result.vertices.size());
    }
    result.center = center;
    
    return result;
}

//--------------------------------------------------
// 表面提取与分析功能
//--------------------------------------------------

std::vector<Mesh> MeshProcessor::extractSurfaces(float angleThreshold) {
    std::vector<Mesh> surfaces;
    
    if (m_model->getMeshes().empty()) {
        std::cerr << "没有可分析的网格数据!" << std::endl;
        return surfaces;
    }
    
    // 获取第一个网格进行处理
    const Mesh& sourceMesh = m_model->getMeshes()[0];
    
    // 收集所有三角形的法线
    std::vector<Vec3> faceNormals;
    for (const auto& tri : sourceMesh.triangles) {
        Vec3 normal = tri.normal;
        if (normal.squared_length() == 0) {
            // 如果三角形没有法线，根据顶点位置计算
            const Vec3& v0 = sourceMesh.vertices[tri.indices[0]].position;
            const Vec3& v1 = sourceMesh.vertices[tri.indices[1]].position;
            const Vec3& v2 = sourceMesh.vertices[tri.indices[2]].position;
            normal = calculateTriangleNormal(v0, v1, v2);
        }
        normal = normal.normalize();
        faceNormals.push_back(normal);
    }
    
    // 基于法线相似度进行DBSCAN聚类
    const float PI = 3.14159265358979323846f;
    float eps = 1.0f - cos(angleThreshold * PI / 180.0f); // 将角度阈值转换为余弦距离
    int minPts = 3; // 最小点数
    std::vector<int> clusterLabels = dbscanClustering(faceNormals, eps, minPts);
    
    // 将面按聚类分组
    std::unordered_map<int, std::vector<unsigned int>> clusters;
    for (unsigned int i = 0; i < clusterLabels.size(); i++) {
        if (clusterLabels[i] != -1) { // -1表示噪声点
            clusters[clusterLabels[i]].push_back(i);
        }
    }
    
    // 为每个聚类创建一个Mesh
    int clusterIdx = 0;
    for (const auto& pair : clusters) {
        int label = pair.first;
        const std::vector<unsigned int>& faceIndices = pair.second;
        
        if (faceIndices.size() < 3) continue; // 忽略太小的聚类
        
        Mesh surfaceMesh;
        surfaceMesh.name = "Surface_" + std::to_string(clusterIdx++);
        
        // 生成随机颜色材质
        Material mat;
        mat.name = "Material_" + std::to_string(label);
        
        // 确保不同表面有不同的颜色
        srand(static_cast<unsigned int>(label * 1000));
        mat.diffuse = Vec3(
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX
        );
        surfaceMesh.material = mat;
        
        // 顶点映射: 原始网格顶点索引 -> 新网格顶点索引
        std::unordered_map<unsigned int, unsigned int> vertexMapping;
        
        // 收集表面的顶点和三角形
        for (unsigned int faceIdx : faceIndices) {
            const Triangle& origTri = sourceMesh.triangles[faceIdx];
            
            Triangle newTri;
            newTri.normal = origTri.normal;
            
            for (int i = 0; i < 3; i++) {
                unsigned int origVertIdx = origTri.indices[i];
                
                // 如果这个顶点还没有添加到新Mesh中
                if (vertexMapping.find(origVertIdx) == vertexMapping.end()) {
                    Vertex newVertex = sourceMesh.vertices[origVertIdx]; // 复制顶点属性
                    vertexMapping[origVertIdx] = surfaceMesh.vertices.size();
                    surfaceMesh.vertices.push_back(newVertex);
                }
                
                // 设置新的三角形索引
                newTri.indices[i] = vertexMapping[origVertIdx];
            }
            
            surfaceMesh.triangles.push_back(newTri);
        }
        
        // 计算表面中心点
        Vec3 center(0.0f, 0.0f, 0.0f);
        for (const auto& vertex : surfaceMesh.vertices) {
            center = center + vertex.position;
        }
        if (!surfaceMesh.vertices.empty()) {
            center = center / static_cast<float>(surfaceMesh.vertices.size());
        }
        surfaceMesh.center = center;
        
        // 添加到结果中
        surfaces.push_back(surfaceMesh);
    }
    
    std::cout << "提取出 " << surfaces.size() << " 个表面（基于法线聚类）" << std::endl;
    return surfaces;
}

std::vector<Mesh> MeshProcessor::extractSurfacesByRegionGrowing(float angleThreshold) {
    std::vector<Mesh> surfaces;
    
    if (m_model->getMeshes().empty()) {
        std::cerr << "没有可分析的网格数据!" << std::endl;
        return surfaces;
    }
    
    // 默认情况下使用更宽松的角度阈值（如果参数过小）
    if (angleThreshold < 5.0f) {
        std::cout << "警告: 角度阈值太小，调整为 20 度以提高表面提取效率" << std::endl;
        angleThreshold = 20.0f;
    }
    
    // 获取第一个网格进行处理
    const Mesh& sourceMesh = m_model->getMeshes()[0];
    
    std::cout << "区域生长: 开始处理网格, 共有 " << sourceMesh.triangles.size() << " 个三角形, "
              << sourceMesh.vertices.size() << " 个顶点" << std::endl;
    
    if (sourceMesh.triangles.empty()) {
        std::cerr << "网格中没有三角形!" << std::endl;
        return surfaces;
    }
    
    // 计算所有面的法线
    std::vector<Vec3> faceNormals;
    int invalidNormals = 0;
    for (size_t i = 0; i < sourceMesh.triangles.size(); i++) {
        Vec3 normal = sourceMesh.triangles[i].normal;
        if (normal.squared_length() < 0.000001f) { // 几乎为零的法线
            normal = calculateFaceNormal(sourceMesh, i);
            invalidNormals++;
        }
        
        // 即使法线长度接近零，calculateFaceNormal也会返回一个有效的法线
        faceNormals.push_back(normal);
    }
    
    std::cout << "区域生长: 已计算 " << faceNormals.size() << " 个面法线，其中修复了 " 
              << invalidNormals << " 个无效法线" << std::endl;
      // 构建面-面邻接图（通过共享边）
    std::vector<std::vector<unsigned int>> faceAdjacency(sourceMesh.triangles.size());
    
    // 创建边到面的映射
    std::unordered_map<uint64_t, std::vector<unsigned int>> edgeToFaces;    // 辅助函数：为边生成唯一的哈希值
    auto edgeHash = [&sourceMesh](unsigned int v1, unsigned int v2) -> uint64_t {
        // 确保索引不会超出32位边界导致哈希冲突
        if (v1 > 0xFFFFFFFF || v2 > 0xFFFFFFFF) {
            std::cerr << "警告: 索引值过大，可能导致哈希冲突: " << v1 << ", " << v2 << std::endl;
            // 对于超大值，我们取模以避免溢出
            v1 = v1 & 0xFFFFFF; // 保留低24位
            v2 = v2 & 0xFFFFFF; // 保留低24位
        }
        
        // 使用顶点的几何位置而不是索引来计算哈希，以处理具有相同坐标但不同索引的顶点
        const Vec3& pos1 = sourceMesh.vertices[v1].position;
        const Vec3& pos2 = sourceMesh.vertices[v2].position;
        
        // 确保顺序一致性（始终使用坐标较小的顶点作为"第一个"顶点）
        const Vec3 *minPos, *maxPos;
        if (pos1.x < pos2.x || (pos1.x == pos2.x && pos1.y < pos2.y) || 
            (pos1.x == pos2.x && pos1.y == pos2.y && pos1.z < pos2.z)) {
            minPos = &pos1;
            maxPos = &pos2;
        } else {
            minPos = &pos2;
            maxPos = &pos1;
        }
        
        // 通过组合两个顶点的浮点坐标来创建哈希值
        // 为了简化，我们使用从浮点到整数的转换并结合XOR
        union {
            float f[3];
            uint32_t i[3];
        } min_conv, max_conv;
        
        min_conv.f[0] = minPos->x;
        min_conv.f[1] = minPos->y;
        min_conv.f[2] = minPos->z;
        
        max_conv.f[0] = maxPos->x;
        max_conv.f[1] = maxPos->y;
        max_conv.f[2] = maxPos->z;
        
        // 组合哈希值
        uint64_t hashA = (static_cast<uint64_t>(min_conv.i[0]) << 32) | min_conv.i[1];
        uint64_t hashB = (static_cast<uint64_t>(min_conv.i[2]) << 32) | max_conv.i[0];
        uint64_t hashC = (static_cast<uint64_t>(max_conv.i[1]) << 32) | max_conv.i[2];
        
        // 混合哈希
        return hashA ^ (hashB + 0x9e3779b9 + (hashA << 6) + (hashA >> 2)) ^ hashC;
    };
    
    // 记录面数据是否有效
    int invalidFaces = 0;
    
    for (unsigned int faceIdx = 0; faceIdx < sourceMesh.triangles.size(); faceIdx++) {
        const Triangle& tri = sourceMesh.triangles[faceIdx];
          // 检查三角形索引是否有效
        bool isValid = true;
        for (int i = 0; i < 3; i++) {
            if (tri.indices[i] < 0 || static_cast<size_t>(tri.indices[i]) >= sourceMesh.vertices.size()) {
                std::cerr << "错误: 三角形 " << faceIdx << " 索引无效: " 
                          << tri.indices[0] << ", " << tri.indices[1] << ", " << tri.indices[2]
                          << ", 但顶点总数为 " << sourceMesh.vertices.size() << std::endl;
                isValid = false;
                invalidFaces++;
                break;
            }
        }
        
        // 检查是否为退化三角形（边长接近为0）
        if (isValid) {
            const float MIN_EDGE_LEN = 0.00001f;
            const Vec3& v0 = sourceMesh.vertices[tri.indices[0]].position;
            const Vec3& v1 = sourceMesh.vertices[tri.indices[1]].position;
            const Vec3& v2 = sourceMesh.vertices[tri.indices[2]].position;
            
            float len1 = (v1 - v0).length();
            float len2 = (v2 - v1).length();
            float len3 = (v0 - v2).length();
            
            if (len1 < MIN_EDGE_LEN || len2 < MIN_EDGE_LEN || len3 < MIN_EDGE_LEN) {
                std::cout << "警告: 三角形 " << faceIdx << " 边长过小: " 
                         << len1 << ", " << len2 << ", " << len3 << std::endl;
                // 不要立即排除它，只是记录警告
            }
        }
        
        if (!isValid) continue;        // 将边添加到映射，使用几何位置而不仅仅是索引
        for (int i = 0; i < 3; i++) {
            // 确保使用的是正确的数据类型，并处理可能的负值
            int idx1 = tri.indices[i];
            int idx2 = tri.indices[(i + 1) % 3];
            
            // 跳过无效索引（负数或越界）
            if (idx1 < 0 || idx2 < 0 || 
                static_cast<size_t>(idx1) >= sourceMesh.vertices.size() || 
                static_cast<size_t>(idx2) >= sourceMesh.vertices.size()) {
                std::cerr << "警告: 三角形 " << faceIdx << " 含有无效索引: " 
                          << idx1 << ", " << idx2 << std::endl;
                isValid = false;
                continue;
            }
            
            unsigned int v1 = static_cast<unsigned int>(idx1);
            unsigned int v2 = static_cast<unsigned int>(idx2);
            
            // 检查是否是退化边（点重合）
            float edgeLen = (sourceMesh.vertices[v1].position - sourceMesh.vertices[v2].position).length();
            if (edgeLen < 0.0001f) {
                std::cerr << "警告: 三角形 " << faceIdx << " 有退化边，顶点 " << v1 << " 和 " << v2 << " 重合" << std::endl;
                // 仍然添加，但记录警告
            }
            
            // 使用基于几何位置的哈希函数
            uint64_t hash = edgeHash(v1, v2);
            edgeToFaces[hash].push_back(faceIdx);
        }
    }
      if (invalidFaces > 0) {
        std::cerr << "警告: 发现 " << invalidFaces << " 个无效三角形" << std::endl;
    }
    
    // 分析边到面的映射情况
    std::unordered_map<int, int> edgeFaceCountDist; // 每条边连接的面数分布
    for (const auto& pair : edgeToFaces) {
        edgeFaceCountDist[pair.second.size()]++;
    }
    
    std::cout << "区域生长: 已构建边到面的映射，共有 " << edgeToFaces.size() << " 条边" << std::endl;
    std::cout << "边连接面数分布: ";
    for (const auto& pair : edgeFaceCountDist) {
        std::cout << pair.first << "个面: " << pair.second << "条边, ";
    }
    std::cout << std::endl;
    
    // 从边缘映射构建邻接图
    int sharedEdgeCount = 0;
    for (const auto& pair : edgeToFaces) {
        const auto& faces = pair.second;
        if (faces.size() == 2) {  // 共享边的两个面
            faceAdjacency[faces[0]].push_back(faces[1]);
            faceAdjacency[faces[1]].push_back(faces[0]);
            sharedEdgeCount++;
        }
    }
      std::cout << "区域生长: 找到 " << sharedEdgeCount << " 条共享边，构建邻接图完成" << std::endl;
    
    // 验证邻接图
    int facesWithNeighbors = 0;
    for (const auto& neighbors : faceAdjacency) {
        if (!neighbors.empty()) {
            facesWithNeighbors++;
        }
    }
    std::cout << "区域生长: " << facesWithNeighbors << " 个面有邻居，" 
              << (sourceMesh.triangles.size() - facesWithNeighbors) << " 个面没有邻居" << std::endl;

    // 如果太多三角形没有邻居，使用备用策略（基于顶点相邻关系）
    if (facesWithNeighbors < sourceMesh.triangles.size() * 0.5f) {
        std::cout << "警告: 超过一半的三角形没有邻居，使用备用策略重建邻接图" << std::endl;
        
        // 构建顶点到面的映射
        std::vector<std::vector<unsigned int>> vertexToFaces(sourceMesh.vertices.size());
        for (unsigned int faceIdx = 0; faceIdx < sourceMesh.triangles.size(); faceIdx++) {
            const Triangle& tri = sourceMesh.triangles[faceIdx];
            for (int i = 0; i < 3; i++) {
                if (tri.indices[i] < sourceMesh.vertices.size()) {
                    vertexToFaces[tri.indices[i]].push_back(faceIdx);
                }
            }
        }
        
        // 重新构建邻接图（基于共享顶点）
        faceAdjacency.clear();
        faceAdjacency.resize(sourceMesh.triangles.size());
        
        for (unsigned int faceIdx = 0; faceIdx < sourceMesh.triangles.size(); faceIdx++) {
            const Triangle& tri = sourceMesh.triangles[faceIdx];
            std::set<unsigned int> neighborFaces;
            
            // 收集共享任何顶点的面
            for (int i = 0; i < 3; i++) {
                unsigned int vertIdx = tri.indices[i];
                if (vertIdx < sourceMesh.vertices.size()) {
                    for (unsigned int adjFace : vertexToFaces[vertIdx]) {
                        if (adjFace != faceIdx) {
                            neighborFaces.insert(adjFace);
                        }
                    }
                }
            }
            
            // 只添加法线相似的邻居
            for (unsigned int adjFace : neighborFaces) {
                // 检查法线相似度（使用更宽松的阈值）
                float dotProduct = faceNormals[faceIdx].dot(faceNormals[adjFace]);
                if (dotProduct > cos(45.0f * 3.14159f / 180.0f)) { // 45度阈值
                    faceAdjacency[faceIdx].push_back(adjFace);
                }
            }
        }
        
        // 重新检查邻接图情况
        facesWithNeighbors = 0;
        for (const auto& neighbors : faceAdjacency) {
            if (!neighbors.empty()) {
                facesWithNeighbors++;
            }
        }
        std::cout << "区域生长(备用策略): " << facesWithNeighbors << " 个面有邻居，"
                  << (sourceMesh.triangles.size() - facesWithNeighbors) << " 个面没有邻居" << std::endl;
    }    // 区域生长算法
    std::vector<bool> processed(sourceMesh.triangles.size(), false);
    const float PI = 3.14159265358979323846f;
    float cosThreshold = cos(angleThreshold * PI / 180.0f);
    float adaptiveThreshold = cosThreshold; // 初始使用指定阈值
    
    std::cout << "区域生长: 角度阈值 = " << angleThreshold << " 度, cos阈值 = " << cosThreshold << std::endl;
    
    // 存储找到的各个连通表面
    std::vector<std::vector<unsigned int>> connectedSurfaces;
    
    // 计算所有法线的平均值，以便后续自适应调整阈值
    Vec3 avgNormal(0.0f, 0.0f, 0.0f);
    for (const auto& normal : faceNormals) {
        avgNormal = avgNormal + normal;
    }
    if (!faceNormals.empty()) {
        avgNormal = avgNormal / static_cast<float>(faceNormals.size());
        avgNormal = avgNormal.normalize();
    }
      // 第一遍使用正常阈值
    int passCount = 1;
    float currentCosThreshold = cosThreshold;
    
    for (int pass = 0; pass < 2; pass++) {
        bool foundAnySurface = false;
        
        for (unsigned int seedFace = 0; seedFace < sourceMesh.triangles.size(); seedFace++) {
            if (processed[seedFace]) continue;
            
            std::vector<unsigned int> currentRegion;
            std::queue<unsigned int> queue;
            queue.push(seedFace);
            processed[seedFace] = true;
            
            while (!queue.empty()) {
                unsigned int currentFace = queue.front();
                queue.pop();
                currentRegion.push_back(currentFace);
                
                // 检查所有邻接面
                for (unsigned int adjFace : faceAdjacency[currentFace]) {
                    if (!processed[adjFace]) {
                        // 检查法线相似度
                        float dotProduct = faceNormals[currentFace].dot(faceNormals[adjFace]);
                        
                        if (dotProduct >= currentCosThreshold) {
                            queue.push(adjFace);
                            processed[adjFace] = true;
                        }
                    }
                }
            }
                // 仅添加符合条件的区域
            if (currentRegion.size() >= 3) {  // 至少包含3个三角形
                // 注意：使用std::move后currentRegion会变空，所以先保存大小
                size_t regionSize = currentRegion.size();
                connectedSurfaces.push_back(std::move(currentRegion));
                std::cout << "区域生长(Pass " << passCount << "): 找到第 " << connectedSurfaces.size() 
                          << " 个表面，包含 " << regionSize << " 个三角形" << std::endl;
                foundAnySurface = true;
            } else if (!currentRegion.empty()) {
                // 释放已处理标记，让这些小区域在后续更宽松的阈值中可能被合并
                for (unsigned int faceIdx : currentRegion) {
                    processed[faceIdx] = false;
                }
            }
        }
        
        // 如果第一遍没找到任何表面，或者有大量未处理的三角形，使用更宽松的阈值再来一遍
        int unprocessedCount = 0;
        for (bool p : processed) {
            if (!p) unprocessedCount++;
        }
        
        // 如果没找到表面或者还有很多未处理的面，尝试第二遍
        if ((pass == 0) && (!foundAnySurface || unprocessedCount > sourceMesh.triangles.size() * 0.3)) {
            currentCosThreshold = cos(std::min(angleThreshold * 1.5f, 45.0f) * PI / 180.0f);
            std::cout << "区域生长: 第一遍未找到足够表面，使用更宽松的阈值(cos=" 
                      << currentCosThreshold << ")再次尝试" << std::endl;
            passCount++;
        } else {
            // 已找到足够表面或已完成第二遍
            break;
        }
    }
    
    // 如果仍有未处理的三角形，将它们放入单独的"噪声"表面
    std::vector<unsigned int> noiseRegion;
    for (unsigned int faceIdx = 0; faceIdx < sourceMesh.triangles.size(); faceIdx++) {
        if (!processed[faceIdx]) {
            noiseRegion.push_back(faceIdx);
        }
    }
    
    if (!noiseRegion.empty()) {
        connectedSurfaces.push_back(std::move(noiseRegion));
        std::cout << "区域生长: 将 " << noiseRegion.size() << " 个未分类三角形归为噪声表面" << std::endl;
    }
    
    // 为每个连通表面创建一个Mesh
    for (size_t surfaceIdx = 0; surfaceIdx < connectedSurfaces.size(); surfaceIdx++) {
        const auto& faceIndices = connectedSurfaces[surfaceIdx];
        if (faceIndices.size() < 3) continue; // 忽略太小的表面
        
        Mesh surfaceMesh;
        surfaceMesh.name = "ConnectedSurface_" + std::to_string(surfaceIdx);
        
        // 生成随机颜色材质
        Material mat;
        mat.name = "Material_" + std::to_string(surfaceIdx);
        
        // 确保不同表面有不同的颜色
        srand(static_cast<unsigned int>(surfaceIdx * 1000));
        mat.diffuse = Vec3(
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX
        );
        surfaceMesh.material = mat;
        
        // 顶点映射: 原始网格顶点索引 -> 新网格顶点索引
        std::unordered_map<unsigned int, unsigned int> vertexMapping;
        
        // 收集表面的顶点和三角形
        for (unsigned int faceIdx : faceIndices) {
            const Triangle& origTri = sourceMesh.triangles[faceIdx];
            
            Triangle newTri;
            newTri.normal = origTri.normal;
            
            for (int i = 0; i < 3; i++) {
                unsigned int origVertIdx = origTri.indices[i];
                
                // 如果这个顶点还没有添加到新Mesh中
                if (vertexMapping.find(origVertIdx) == vertexMapping.end()) {
                    Vertex newVertex = sourceMesh.vertices[origVertIdx]; // 复制顶点属性
                    vertexMapping[origVertIdx] = static_cast<unsigned int>(surfaceMesh.vertices.size());
                    surfaceMesh.vertices.push_back(newVertex);
                }
                
                // 设置新的三角形索引
                newTri.indices[i] = vertexMapping[origVertIdx];
            }
            
            surfaceMesh.triangles.push_back(newTri);
        }
        
        // 计算表面中心点
        Vec3 center(0.0f, 0.0f, 0.0f);
        for (const auto& vertex : surfaceMesh.vertices) {
            center = center + vertex.position;
        }
        if (!surfaceMesh.vertices.empty()) {
            center = center / static_cast<float>(surfaceMesh.vertices.size());
        }
        surfaceMesh.center = center;
        
        // 添加到结果中
        surfaces.push_back(surfaceMesh);
    }
    
    std::cout << "提取出 " << surfaces.size() << " 个表面（基于区域生长）" << std::endl;
    return surfaces;
}

Mesh MeshProcessor::findTopSurface(int upAxis) {
    // 确保有可用数据
    if (m_model->getMeshes().empty()) {
        std::cerr << "没有可分析的网格数据!" << std::endl;
        return Mesh();
    }
    
    // 定义上向量（默认为z轴正方向）
    Vec3 upVector(0.0f, 0.0f, 0.0f);
    switch (upAxis) {
        case 0: upVector.x = 1.0f; break; // X轴
        case 1: upVector.y = 1.0f; break; // Y轴
        case 2: default: upVector.z = 1.0f; break; // Z轴（默认）
    }
    
    // 首先用基于区域生长的方法提取表面
    std::vector<Mesh> surfaces = extractSurfacesByRegionGrowing(15.0f); // 15度角度阈值
    
    // 如果没有找到任何表面，返回空Mesh
    if (surfaces.empty()) {
        std::cerr << "没有找到任何连续表面!" << std::endl;
        return Mesh();
    }
    
    // 找到最适合作为顶面的表面
    float bestScore = -2.0f; // 初始值小于-1确保任何表面都能更新
    int bestIndex = -1;
    float highestPoint = -std::numeric_limits<float>::max();
    
    for (size_t i = 0; i < surfaces.size(); i++) {
        // 计算表面与上向量的对齐度
        float score = calculateNormalScore(surfaces[i], upVector);
        
        // 如果表面足够平坦并且朝上
        if (score > 0.7f) { // cos(45度) ≈ 0.7071
            // 计算表面的最高点（在上方向上）
            float maxHeight = -std::numeric_limits<float>::max();
            for (const auto& vertex : surfaces[i].vertices) {
                float height = 0.0f;
                switch (upAxis) {
                    case 0: height = vertex.position.x; break;
                    case 1: height = vertex.position.y; break;
                    case 2: default: height = vertex.position.z; break;
                }
                maxHeight = std::max(maxHeight, height);
            }
            
            // 如果这个表面是最高的朝上表面，或者得分显著更好
            if (maxHeight > highestPoint || (std::abs(maxHeight - highestPoint) < 0.01f && score > bestScore)) {
                bestScore = score;
                bestIndex = static_cast<int>(i);
                highestPoint = maxHeight;
            }
        }
    }
    
    // 如果找到了合适的顶面
    if (bestIndex >= 0) {
        m_topSurface = surfaces[bestIndex];
        std::cout << "找到顶面，包含 " << m_topSurface.triangles.size() << " 个三角形" << std::endl;
        std::cout << "顶面法线得分: " << bestScore << std::endl;
        return m_topSurface;
    }
    
    std::cout << "未找到合适的顶面，返回最大表面" << std::endl;
    
    // 如果没有理想的顶面，则选择最大的表面
    size_t maxSize = 0;
    size_t maxIndex = 0;
    for (size_t i = 0; i < surfaces.size(); i++) {
        if (surfaces[i].triangles.size() > maxSize) {
            maxSize = surfaces[i].triangles.size();
            maxIndex = i;
        }
    }
    
    m_topSurface = surfaces[maxIndex];
    return m_topSurface;
}

// 计算面的平均高度
float MeshProcessor::calculateFaceHeight(const Mesh& mesh, unsigned int faceIndex, int upAxis) {
    const Triangle& tri = mesh.triangles[faceIndex];
    float sum = 0.0f;
    
    for (int i = 0; i < 3; i++) {
        const Vec3& pos = mesh.vertices[tri.indices[i]].position;
        switch (upAxis) {
            case 0: sum += pos.x; break; // X轴
            case 1: sum += pos.y; break; // Y轴
            case 2: default: sum += pos.z; break; // Z轴（默认）
        }
    }
    
    return sum / 3.0f; // 三个顶点的平均高度
}

// 计算网格与上向量的对齐程度
float MeshProcessor::calculateNormalScore(const Mesh& mesh, const Vec3& upVector) {
    if (mesh.triangles.empty()) return 0.0f;
    
    // 计算平均法线
    Vec3 avgNormal(0.0f, 0.0f, 0.0f);
    
    // 每个三角形的面积作为权重
    float totalArea = 0.0f;
    
    for (const auto& tri : mesh.triangles) {
        // 获取三角形顶点
        const Vec3& v0 = mesh.vertices[tri.indices[0]].position;
        const Vec3& v1 = mesh.vertices[tri.indices[1]].position;
        const Vec3& v2 = mesh.vertices[tri.indices[2]].position;
        
        // 计算三角形面积（使用叉乘的一半）
        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;
        Vec3 crossProduct = edge1.cross(edge2);
        float area = 0.5f * crossProduct.length();
        
        // 使用面积加权法线
        Vec3 normal = tri.normal;
        if (normal.squared_length() == 0) {
            normal = calculateTriangleNormal(v0, v1, v2);
        }
        
        avgNormal = avgNormal + normal * area;
        totalArea += area;
    }
    
    // 归一化平均法线
    if (totalArea > 0.0f) {
        avgNormal = avgNormal / totalArea;
    }
    
    avgNormal = avgNormal.normalize();
    
    // 计算与上向量的点积（cos角度）
    float dot = avgNormal.dot(upVector);
    
    // 返回与上向量的对齐程度
    return dot; // 1表示完全对齐，-1表示完全相反
}

// 计算两个标准化法线向量之间的距离（使用余弦距离）
float MeshProcessor::normalDistance(const Vec3& a, const Vec3& b) {
    // 对于单位向量，dot product = cos(angle)
    float cosAngle = a.dot(b);
    // 限制在[-1, 1]范围内
    cosAngle = std::max(-1.0f, std::min(1.0f, cosAngle));
    // 距离 = 1 - cos(angle)，范围是[0, 2]，0表示完全相同方向，2表示完全相反方向
    return 1.0f - cosAngle;
}

// 找到eps距离内的所有邻居点
std::vector<int> MeshProcessor::findNeighbors(const std::vector<Vec3>& normals, int pointIdx, float eps) {
    std::vector<int> neighbors;
    for (size_t i = 0; i < normals.size(); i++) {
        if (i != static_cast<size_t>(pointIdx)) {
            float dist = normalDistance(normals[pointIdx], normals[i]);
            if (dist <= eps) {
                neighbors.push_back(static_cast<int>(i));
            }
        }
    }
    return neighbors;
}

// DBSCAN聚类算法实现
std::vector<int> MeshProcessor::dbscanClustering(const std::vector<Vec3>& normals, float eps, int minPts) {
    // 初始化标签，-1表示未分类，-2表示噪声
    std::vector<int> labels(normals.size(), -1);
    int clusterID = 0;
    
    // 遍历所有点
    for (size_t i = 0; i < normals.size(); i++) {
        // 跳过已被分类的点
        if (labels[i] != -1) continue;
        
        // 寻找邻居
        std::vector<int> neighbors = findNeighbors(normals, static_cast<int>(i), eps);
        
        // 如果邻居数量不足，标记为噪声
        if (static_cast<int>(neighbors.size()) < minPts) {
            labels[i] = -2;
            continue;
        }
        
        // 开始一个新的聚类
        labels[i] = clusterID;
        
        // 处理邻居
        std::vector<int> seedSet(neighbors);
        
        for (size_t j = 0; j < seedSet.size(); j++) {
            int currentPt = seedSet[j];
            
            // 如果是噪声点，将其归类到当前聚类
            if (labels[currentPt] == -2) {
                labels[currentPt] = clusterID;
            }
            
            // 如果已被分类，继续处理下一个
            if (labels[currentPt] != -1) continue;
            
            // 将此点添加到当前聚类
            labels[currentPt] = clusterID;
            
            // 寻找这个点的邻居
            std::vector<int> currentNeighbors = findNeighbors(normals, currentPt, eps);
            
            // 如果邻居数量足够，将其添加到种子集合
            if (static_cast<int>(currentNeighbors.size()) >= minPts) {
                for (int neighbor : currentNeighbors) {
                    seedSet.push_back(neighbor);
                }
            }
        }
        
        // 完成一个聚类，增加聚类ID
        clusterID++;
    }
    
    // 将噪声点（-2）更改为-1
    for (auto& label : labels) {
        if (label == -2) label = -1;
    }
    
    return labels;
}

// 计算面法线
Vec3 MeshProcessor::calculateFaceNormal(const Mesh& mesh, unsigned int faceIndex) {
    const Triangle& tri = mesh.triangles[faceIndex];
    const Vec3& v0 = mesh.vertices[tri.indices[0]].position;
    const Vec3& v1 = mesh.vertices[tri.indices[1]].position;
    const Vec3& v2 = mesh.vertices[tri.indices[2]].position;
    
    // 计算两条边的向量
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    
    // 检查边是否有效（非零长度）
    if (edge1.squared_length() < 0.000001f || edge2.squared_length() < 0.000001f) {
        std::cout << "警告: 三角形 " << faceIndex << " 有退化边" << std::endl;
        return Vec3(0, 0, 1); // 返回默认向上的法线
    }
    
    // 计算叉积得到法线向量
    Vec3 normal = edge1.cross(edge2);
    
    // 检查法线长度，防止退化三角形
    float length = normal.length();
    if (length < 0.000001f) {
        std::cout << "警告: 三角形 " << faceIndex << " 法线接近零向量" << std::endl;
        return Vec3(0, 0, 1); // 返回默认向上的法线
    }
    
    return normal / length; // 显式归一化，避免使用normalize()可能产生的问题
}

// 为其他私有辅助函数添加实现
// ...
