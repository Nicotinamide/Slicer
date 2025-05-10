#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <random>
#include <iostream>
#include <set>
#include <map>
#include <queue>

#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/Exporter.hpp>  // Add this include for Exporter class

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class SurfaceAnalyzer {
    public:
        // Main function to segment surfaces and find top surface
        const aiMesh* findTopSurface(const aiScene* scene, int upAxis = 2);
        
        // Export the top surface as a separate mesh
        bool exportTopSurface(const std::string& filename);

        // 新增：分离所有面
        std::vector<aiMesh*> separateFaces(const aiScene* scene);
        
        // 新增：从分离的面中提取顶面
        const aiMesh* extractTopSurfaceFromFaces(const std::vector<aiMesh*>& faces, int upAxis = 2);
        
        // 新增：清理分离的面
        void cleanupSeparatedFaces(std::vector<aiMesh*>& faces);

        
        std::vector<std::vector<unsigned int>> SurfaceAnalyzer::extractSurfaces(const aiScene* scene, float angleThreshold);

        std::vector<std::vector<unsigned int>> SurfaceAnalyzer::extractSurfacesByRegionGrowing(const aiScene* scene, float angleThreshold);

        aiMesh* SurfaceAnalyzer::createColoredSurfaceMesh(const aiScene* scene, 
            const std::vector<std::vector<unsigned int>>& surfaces);
    
    private:
        // Structure for storing submesh data
        struct SubMesh {
            std::vector<aiVector3D> vertices;
            std::vector<unsigned int> indices;
            std::vector<aiVector3D> normals;
            float normalScore;
        };
        
        // DBSCAN clustering implementation
        std::vector<int> dbscanClustering(const std::vector<aiVector3D>& normals, float eps, int minPts);
        
        // Calculate distance between two normalized vectors
        float normalDistance(const aiVector3D& a, const aiVector3D& b);
        
        // Find neighbors within eps distance
        std::vector<int> findNeighbors(const std::vector<aiVector3D>& normals, int pointIdx, float eps);
        
        // Extract submeshes based on clustering results
        std::vector<SubMesh> extractSubmeshes(const aiMesh* mesh, const std::vector<int>& labels);
        
        // Calculate normal score (alignment with up vector)
        float calculateNormalScore(const SubMesh& submesh, const aiVector3D& upVector);

        // 新增：计算面的法线
        aiVector3D calculateFaceNormal(const aiMesh* mesh, unsigned int faceIndex);
        
        // 新增：计算面的平均高度
        float calculateFaceHeight(const aiMesh* mesh, unsigned int faceIndex, int upAxis);

        
        // Store the identified top surface
        SubMesh m_topSurface;
        
        // Assimp mesh that represents the top surface
        aiMesh* m_topSurfaceMesh = nullptr;
    };