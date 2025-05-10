#include "surface_analyzer.h"

const aiMesh* SurfaceAnalyzer::findTopSurface(const aiScene* scene, int upAxis) {
    if (!scene || !scene->HasMeshes()) {
        std::cerr << "Invalid scene or no meshes found!" << std::endl;
        return nullptr;
    }

    // 1. 分离所有面
    std::vector<aiMesh*> separatedFaces = separateFaces(scene);
    
    // 2. 从分离的面中提取顶面
    const aiMesh* topSurface = extractTopSurfaceFromFaces(separatedFaces, upAxis);
    
    // 3. 清理分离的面
    cleanupSeparatedFaces(separatedFaces);
    
    return topSurface;
}

std::vector<int> SurfaceAnalyzer::dbscanClustering(const std::vector<aiVector3D>& normals, float eps, int minPts) {
    // Initialize all points as unclassified (-1)
    std::vector<int> labels(normals.size(), -1);
    int clusterID = 0;
    
    // Iterate through all points
    for (size_t i = 0; i < normals.size(); i++) {
        // Skip already classified points
        if (labels[i] != -1) continue;
        
        // Find neighbors
        std::vector<int> neighbors = findNeighbors(normals, i, eps);
        
        // Check if this is a core point
        if (neighbors.size() < static_cast<size_t>(minPts)) {
            labels[i] = -1; // Noise
            continue;
        }
        
        // Start a new cluster
        labels[i] = clusterID;
        
        // Process neighbors
        std::vector<int> seedSet = neighbors;
        
        for (size_t j = 0; j < seedSet.size(); j++) {
            int currentPt = seedSet[j];
            
            // Mark noise points as part of the cluster
            if (labels[currentPt] == -1) {
                labels[currentPt] = clusterID;
            }
            
            // Skip already classified points
            if (labels[currentPt] != -1) continue;
            
            // Add point to cluster
            labels[currentPt] = clusterID;
            
            // Find neighbors of current point
            std::vector<int> currentNeighbors = findNeighbors(normals, currentPt, eps);
            
            // Add new neighbors to seed set
            if (currentNeighbors.size() >= static_cast<size_t>(minPts)) {
                seedSet.insert(seedSet.end(), currentNeighbors.begin(), currentNeighbors.end());
            }
        }
        
        clusterID++; // Next cluster
    }
    
    return labels;
}

float SurfaceAnalyzer::normalDistance(const aiVector3D& a, const aiVector3D& b) {
    // Cosine distance for normalized vectors
    float dot = a.x * b.x + a.y * b.y + a.z * b.z;
    return 1.0f - dot; // 0 means identical, 2 means opposite
}

std::vector<int> SurfaceAnalyzer::findNeighbors(const std::vector<aiVector3D>& normals, int pointIdx, float eps) {
    std::vector<int> neighbors;
    
    for (size_t i = 0; i < normals.size(); i++) {
        if (normalDistance(normals[pointIdx], normals[i]) < eps) {
            neighbors.push_back(i);
        }
    }
    
    return neighbors;
}

std::vector<SurfaceAnalyzer::SubMesh> SurfaceAnalyzer::extractSubmeshes(const aiMesh* mesh, const std::vector<int>& labels) {
    // Find unique labels (clusters)
    std::set<int> uniqueLabels;
    for (int label : labels) {
        if (label != -1) uniqueLabels.insert(label);
    }
    
    // Create a submesh for each label
    std::vector<SubMesh> submeshes(uniqueLabels.size());
    std::vector<int> labelToSubmeshIndex;
    
    int idx = 0;
    for (int label : uniqueLabels) {
        labelToSubmeshIndex.push_back(label);
        idx++;
    }
    
    // Extract faces for each submesh
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        
        if (face.mNumIndices == 3 && labels[i] != -1) {
            // Find submesh index for this face
            auto it = std::find(labelToSubmeshIndex.begin(), labelToSubmeshIndex.end(), labels[i]);
            if (it == labelToSubmeshIndex.end()) continue;
            
            int submeshIdx = std::distance(labelToSubmeshIndex.begin(), it);
            SubMesh& submesh = submeshes[submeshIdx];
            
            // Add vertices and indices
            unsigned int baseIndex = submesh.vertices.size();
            
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                unsigned int vertexIdx = face.mIndices[j];
                submesh.vertices.push_back(mesh->mVertices[vertexIdx]);
                
                if (mesh->HasNormals()) {
                    submesh.normals.push_back(mesh->mNormals[vertexIdx]);
                }
                
                submesh.indices.push_back(baseIndex + j);
            }
        }
    }
    
    return submeshes;
}

float SurfaceAnalyzer::calculateNormalScore(const SubMesh& submesh, const aiVector3D& upVector) {
    if (submesh.normals.empty()) return -1.0f;
    
    // Calculate average normal
    aiVector3D avgNormal(0.0f, 0.0f, 0.0f);
    
    for (const auto& normal : submesh.normals) {
        avgNormal += normal;
    }
    
    if (submesh.normals.size() > 0) {
        avgNormal /= static_cast<float>(submesh.normals.size());
    }
    
    avgNormal.Normalize();
    
    // Calculate dot product with up vector
    float dot = avgNormal.x * upVector.x + avgNormal.y * upVector.y + avgNormal.z * upVector.z;
    
    return dot; // 1 means perfectly aligned, -1 means opposite
}

bool SurfaceAnalyzer::exportTopSurface(const std::string& filename) {
    if (!m_topSurfaceMesh) {
        std::cerr << "No top surface to export!" << std::endl;
        return false;
    }
    
    // Create a new scene with just the top surface
    aiScene* scene = new aiScene();
    scene->mRootNode = new aiNode();
    
    // Set up materials (minimal)
    scene->mMaterials = new aiMaterial*[1];
    scene->mMaterials[0] = new aiMaterial();
    scene->mNumMaterials = 1;
    
    // Set up meshes
    scene->mMeshes = new aiMesh*[1];
    scene->mMeshes[0] = m_topSurfaceMesh;
    scene->mNumMeshes = 1;
    
    // Set up mesh references in root node
    scene->mRootNode->mMeshes = new unsigned int[1];
    scene->mRootNode->mMeshes[0] = 0;
    scene->mRootNode->mNumMeshes = 1;
    
    // Export using Assimp
    Assimp::Exporter exporter;
    aiReturn result = exporter.Export(scene, "stl", filename);
    
    // Clean up (except m_topSurfaceMesh which is still being used)
    delete scene->mRootNode->mMeshes;
    delete scene->mRootNode;
    delete[] scene->mMaterials[0];
    delete[] scene->mMaterials;
    delete[] scene->mMeshes; // Note: not deleting m_topSurfaceMesh
    delete scene;
    
    return (result == aiReturn_SUCCESS);
}

// 计算面的法线
aiVector3D SurfaceAnalyzer::calculateFaceNormal(const aiMesh* mesh, unsigned int faceIndex) {
    const aiFace& face = mesh->mFaces[faceIndex];
    if (face.mNumIndices != 3) {
        return aiVector3D(0.0f, 0.0f, 0.0f);
    }

    aiVector3D v1 = mesh->mVertices[face.mIndices[1]] - mesh->mVertices[face.mIndices[0]];
    aiVector3D v2 = mesh->mVertices[face.mIndices[2]] - mesh->mVertices[face.mIndices[0]];
    aiVector3D normal = v1 ^ v2;
    normal.Normalize();
    return normal;
}

// 计算面的平均高度
float SurfaceAnalyzer::calculateFaceHeight(const aiMesh* mesh, unsigned int faceIndex, int upAxis) {
    const aiFace& face = mesh->mFaces[faceIndex];
    if (face.mNumIndices != 3) {
        return 0.0f;
    }

    float height = 0.0f;
    for (unsigned int i = 0; i < 3; i++) {
        height += mesh->mVertices[face.mIndices[i]][upAxis];
    }
    return height / 3.0f;
}

// 分离所有面
std::vector<aiMesh*> SurfaceAnalyzer::separateFaces(const aiScene* scene) {
    std::vector<aiMesh*> separatedFaces;
    
    if (!scene || !scene->HasMeshes()) {
        std::cerr << "Invalid scene or no meshes found!" << std::endl;
        return separatedFaces;
    }

    // Random number generator for colors
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.1f, 0.9f);

    // 遍历所有网格
    for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
        const aiMesh* mesh = scene->mMeshes[meshIdx];
        
        // 遍历所有面
        for (unsigned int faceIdx = 0; faceIdx < mesh->mNumFaces; faceIdx++) {
            const aiFace& face = mesh->mFaces[faceIdx];
            
            // 只处理三角形面
            if (face.mNumIndices != 3) continue;
            
            // 创建新的网格
            aiMesh* newMesh = new aiMesh();
            newMesh->mNumVertices = 3;
            newMesh->mVertices = new aiVector3D[3];
            newMesh->mNormals = new aiVector3D[3];
            
            // Add color to the mesh
            newMesh->mColors[0] = new aiColor4D[3];
            aiColor4D faceColor(dist(rng), dist(rng), dist(rng), 1.0f);
            
            // 复制顶点和法线
            for (unsigned int i = 0; i < 3; i++) {
                unsigned int vertexIdx = face.mIndices[i];
                newMesh->mVertices[i] = mesh->mVertices[vertexIdx];
                
                if (mesh->HasNormals()) {
                    newMesh->mNormals[i] = mesh->mNormals[vertexIdx];
                } else {
                    // 如果没有法线，计算面的法线
                    aiVector3D normal = calculateFaceNormal(mesh, faceIdx);
                    newMesh->mNormals[i] = normal;
                }
                
                // Assign color to each vertex
                newMesh->mColors[0][i] = faceColor;
            }
            
            // 设置面
            newMesh->mNumFaces = 1;
            newMesh->mFaces = new aiFace[1];
            aiFace& newFace = newMesh->mFaces[0];
            newFace.mNumIndices = 3;
            newFace.mIndices = new unsigned int[3];
            for (unsigned int i = 0; i < 3; i++) {
                newFace.mIndices[i] = i;
            }
            
            separatedFaces.push_back(newMesh);
        }
    }
    
    std::cout << "Separated " << separatedFaces.size() << " faces" << std::endl;
    return separatedFaces;
}

// 从分离的面中提取顶面
const aiMesh* SurfaceAnalyzer::extractTopSurfaceFromFaces(const std::vector<aiMesh*>& faces, int upAxis) {
    if (faces.empty()) {
        std::cerr << "No faces to analyze!" << std::endl;
        return nullptr;
    }

    // 1. 设定上方向
    aiVector3D upVector(0.0f, 0.0f, 0.0f);
    upVector[upAxis] = 1.0f;

    // 2. 收集所有面的法线和高度
    std::vector<aiVector3D> faceNormals;
    std::vector<float> faceHeights;
    std::vector<aiMesh*> faceMeshes;
    
    for (const auto& face : faces) {
        if (!face || face->mNumFaces != 1) continue;
        
        // 计算法线
        aiVector3D normal = calculateFaceNormal(face, 0);
        if (normal * upVector < 0) normal = -normal;
        
        // 计算高度
        float height = calculateFaceHeight(face, 0, upAxis);
        
        faceNormals.push_back(normal);
        faceHeights.push_back(height);
        faceMeshes.push_back(face);
    }

    // 3. 聚类
    float eps = 0.15f;
    std::vector<int> labels = dbscanClustering(faceNormals, eps, 1);

    // 4. 统计每个聚类的得分
    std::map<int, std::vector<int>> clusterToFaceIdxs;
    for (size_t i = 0; i < labels.size(); ++i) {
        if (labels[i] != -1) clusterToFaceIdxs[labels[i]].push_back(i);
    }
    
    int bestCluster = -1;
    float bestScore = -2.0f;
    
    for (const auto& kv : clusterToFaceIdxs) {
        // 计算平均法线
        aiVector3D avgNormal(0,0,0);
        float avgHeight = 0.0f;
        
        for (int idx : kv.second) {
            avgNormal += faceNormals[idx];
            avgHeight += faceHeights[idx];
        }
        
        avgNormal /= float(kv.second.size());
        avgNormal.Normalize();
        avgHeight /= float(kv.second.size());
        
        // 计算法线得分
        float normalScore = avgNormal * upVector;
        
        // 综合评分（可以调整权重）
        float score = normalScore;
        
        if (score > bestScore) {
            bestScore = score;
            bestCluster = kv.first;
        }
    }
    
    if (bestCluster == -1) {
        std::cerr << "No top surface cluster found!" << std::endl;
        return nullptr;
    }

    // 5. 合并最佳聚类的面
    std::vector<aiVector3D> mergedVertices;
    std::vector<aiVector3D> mergedNormals;
    std::vector<aiColor4D> mergedColors;  // Add storage for colors
    std::vector<unsigned int> mergedIndices;
    unsigned int vertexOffset = 0;
    
    for (int idx : clusterToFaceIdxs[bestCluster]) {
        const aiMesh* face = faceMeshes[idx];
        const aiFace& f = face->mFaces[0];
        
        for (unsigned int j = 0; j < 3; ++j) {
            mergedVertices.push_back(face->mVertices[j]);
            mergedNormals.push_back(face->mNormals[j]);
            
            // Copy colors if available
            if (face->HasVertexColors(0)) {
                mergedColors.push_back(face->mColors[0][j]);
            }
            
            mergedIndices.push_back(vertexOffset++);
        }
    }

    if (mergedVertices.empty() || mergedIndices.empty()) {
        std::cerr << "No top surface found!" << std::endl;
        return nullptr;
    }

    // 6. 构造新的 aiMesh
    m_topSurfaceMesh = new aiMesh();
    m_topSurfaceMesh->mNumVertices = mergedVertices.size();
    m_topSurfaceMesh->mVertices = new aiVector3D[mergedVertices.size()];
    m_topSurfaceMesh->mNormals = new aiVector3D[mergedNormals.size()];
    
    // Add colors array if we have colors
    if (!mergedColors.empty()) {
        m_topSurfaceMesh->mColors[0] = new aiColor4D[mergedVertices.size()];
    }
    
    for (size_t i = 0; i < mergedVertices.size(); ++i) {
        m_topSurfaceMesh->mVertices[i] = mergedVertices[i];
        m_topSurfaceMesh->mNormals[i] = mergedNormals[i];
        
        // Copy colors if available
        if (!mergedColors.empty()) {
            m_topSurfaceMesh->mColors[0][i] = mergedColors[i];
        }
    }
    
    m_topSurfaceMesh->mNumFaces = mergedIndices.size() / 3;
    m_topSurfaceMesh->mFaces = new aiFace[m_topSurfaceMesh->mNumFaces];
    
    for (unsigned int i = 0; i < m_topSurfaceMesh->mNumFaces; i++) {
        aiFace& face = m_topSurfaceMesh->mFaces[i];
        face.mNumIndices = 3;
        face.mIndices = new unsigned int[3];
        face.mIndices[0] = i*3;
        face.mIndices[1] = i*3+1;
        face.mIndices[2] = i*3+2;
    }

    std::cout << "Found top surface with " << m_topSurfaceMesh->mNumVertices
              << " vertices and " << m_topSurfaceMesh->mNumFaces << " faces" << std::endl;
    std::cout << "Normal score: " << bestScore << std::endl;

    return m_topSurfaceMesh;
}

// 清理分离的面
void SurfaceAnalyzer::cleanupSeparatedFaces(std::vector<aiMesh*>& faces) {
    for (auto face : faces) {
        if (face) {
            delete[] face->mVertices;
            delete[] face->mNormals;
            
            // Clean up colors if present
            if (face->HasVertexColors(0)) {
                delete[] face->mColors[0];
            }
            
            delete[] face->mFaces[0].mIndices;
            delete[] face->mFaces;
            delete face;
        }
    }
    faces.clear();
}

std::vector<std::vector<unsigned int>> SurfaceAnalyzer::extractSurfaces(const aiScene* scene, float angleThreshold) {
    std::vector<std::vector<unsigned int>> surfaces; // 存储每个表面包含的面索引
    
    if (!scene || !scene->HasMeshes()) return surfaces;
    
    const aiMesh* mesh = scene->mMeshes[0]; // 处理第一个网格
    
    // 收集所有三角形的法线
    std::vector<aiVector3D> faceNormals;
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiVector3D normal = calculateFaceNormal(mesh, i);
        normal.Normalize();
        faceNormals.push_back(normal);
    }
    
    // 基于法线相似度进行聚类
    float eps = 1.0f - cos(angleThreshold * AI_MATH_PI / 180.0f); // 将角度转换为余弦距离
    std::vector<int> clusterLabels = dbscanClustering(faceNormals, eps, 3);
    
    // 将面按聚类分组
    std::map<int, std::vector<unsigned int>> clusters;
    for (unsigned int i = 0; i < clusterLabels.size(); i++) {
        if (clusterLabels[i] != -1) {
            clusters[clusterLabels[i]].push_back(i);
        }
    }
    
    // 转换为结果格式
    for (const auto& [cluster, faceIndices] : clusters) {
        surfaces.push_back(faceIndices);
    }
    
    std::cout << "提取出 " << surfaces.size() << " 个连续表面" << std::endl;
    return surfaces;
}

std::vector<std::vector<unsigned int>> SurfaceAnalyzer::extractSurfacesByRegionGrowing(const aiScene* scene, float angleThreshold) {
    std::vector<std::vector<unsigned int>> surfaces;
    
    if (!scene || !scene->HasMeshes()) return surfaces;
    
    const aiMesh* mesh = scene->mMeshes[0];
    
    // 计算所有面的法线
    std::vector<aiVector3D> faceNormals;
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        faceNormals.push_back(calculateFaceNormal(mesh, i));
    }
    
    // 构建面-面邻接图
    std::vector<std::vector<unsigned int>> faceAdjacency(mesh->mNumFaces);
    
    // 创建边到面的映射
    std::map<std::pair<unsigned int, unsigned int>, std::vector<unsigned int>> edgeToFaces;
    
    for (unsigned int faceIdx = 0; faceIdx < mesh->mNumFaces; faceIdx++) {
        const aiFace& face = mesh->mFaces[faceIdx];
        if (face.mNumIndices != 3) continue;
        
        // 将边添加到映射
        for (unsigned int i = 0; i < 3; i++) {
            unsigned int v1 = face.mIndices[i];
            unsigned int v2 = face.mIndices[(i + 1) % 3];
            unsigned int minV = std::min(v1, v2);
            unsigned int maxV = std::max(v1, v2);
            
            edgeToFaces[{minV, maxV}].push_back(faceIdx);
        }
    }
    
    // 从边缘映射构建邻接图
    for (const auto& [edge, faces] : edgeToFaces) {
        if (faces.size() == 2) {  // 共享边的两个面
            faceAdjacency[faces[0]].push_back(faces[1]);
            faceAdjacency[faces[1]].push_back(faces[0]);
        }
    }
    
    // 区域生长算法
    std::vector<bool> processed(mesh->mNumFaces, false);
    float cosThreshold = cos(angleThreshold * AI_MATH_PI / 180.0f);
    
    for (unsigned int seedFace = 0; seedFace < mesh->mNumFaces; seedFace++) {
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
                    float dotProduct = faceNormals[currentFace].Normalize() * 
                                      faceNormals[adjFace].Normalize();
                    
                    if (dotProduct >= cosThreshold) {
                        queue.push(adjFace);
                        processed[adjFace] = true;
                    }
                }
            }
        }
        
        if (!currentRegion.empty()) {
            surfaces.push_back(currentRegion);
        }
    }
    
    std::cout << "使用区域生长算法提取出 " << surfaces.size() << " 个连续表面" << std::endl;
    return surfaces;
}

aiMesh* SurfaceAnalyzer::createColoredSurfaceMesh(const aiScene* scene, 
                                               const std::vector<std::vector<unsigned int>>& surfaces) {
    if (!scene || !scene->HasMeshes()) return nullptr;
    
    const aiMesh* originalMesh = scene->mMeshes[0];
    
    // 创建新的网格
    aiMesh* coloredMesh = new aiMesh();
    coloredMesh->mNumVertices = originalMesh->mNumVertices;
    coloredMesh->mVertices = new aiVector3D[originalMesh->mNumVertices];
    coloredMesh->mNormals = new aiVector3D[originalMesh->mNumVertices];
    coloredMesh->mColors[0] = new aiColor4D[originalMesh->mNumVertices];
    
    // 复制顶点和法线
    for (unsigned int i = 0; i < originalMesh->mNumVertices; i++) {
        coloredMesh->mVertices[i] = originalMesh->mVertices[i];
        if (originalMesh->HasNormals()) {
            coloredMesh->mNormals[i] = originalMesh->mNormals[i];
        }
        // 初始化颜色为白色
        coloredMesh->mColors[0][i] = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    // 设置面与颜色
    coloredMesh->mNumFaces = originalMesh->mNumFaces;
    coloredMesh->mFaces = new aiFace[originalMesh->mNumFaces];
    
    // 随机数生成器
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.1f, 0.9f);
    
    // 为每个表面分配颜色
    std::vector<aiColor4D> surfaceColors;
    for (size_t i = 0; i < surfaces.size(); i++) {
        surfaceColors.push_back(aiColor4D(dist(rng), dist(rng), dist(rng), 1.0f));
    }
    
    // 设置面和顶点颜色
    for (size_t surfaceIdx = 0; surfaceIdx < surfaces.size(); surfaceIdx++) {
        const auto& surfaceFaces = surfaces[surfaceIdx];
        aiColor4D surfaceColor = surfaceColors[surfaceIdx];
        
        for (unsigned int faceIdx : surfaceFaces) {
            const aiFace& origFace = originalMesh->mFaces[faceIdx];
            aiFace& newFace = coloredMesh->mFaces[faceIdx];
            
            newFace.mNumIndices = origFace.mNumIndices;
            newFace.mIndices = new unsigned int[origFace.mNumIndices];
            
            for (unsigned int i = 0; i < origFace.mNumIndices; i++) {
                unsigned int vertexIdx = origFace.mIndices[i];
                newFace.mIndices[i] = vertexIdx;
                
                // 设置顶点颜色
                coloredMesh->mColors[0][vertexIdx] = surfaceColor;
            }
        }
    }
    
    return coloredMesh;
}