// src/model_loader.cpp
#include "model_loader.h"
#include "cgal_top_surface.h"
#include <iostream>

#ifdef USE_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// 使用静态导入器以保持场景生命周期
static Assimp::Importer importer;
#endif

const aiScene* loadModelWithAssimp(const std::string& filePath) {
#ifdef USE_ASSIMP
    // 释放之前的资源
    importer.FreeScene();

    // 读取文件，并指定后期处理步骤
    const aiScene* scene = importer.ReadFile(filePath,
                                           aiProcess_Triangulate |
                                           aiProcess_JoinIdenticalVertices |
                                           aiProcess_SortByPType |
                                           aiProcess_GenNormals |
                                           aiProcess_FixInfacingNormals);

    // 检查场景是否加载成功
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp importer error: " << importer.GetErrorString() << std::endl;
        return nullptr;
    }

    // 输出模型信息    std::cout << "Successfully loaded model: " << filePath << std::endl;
    std::cout << "Number of meshes: " << scene->mNumMeshes << std::endl;
    if (scene->mNumMeshes > 0) {
        std::cout << "First mesh has " << scene->mMeshes[0]->mNumVertices << " vertices." << std::endl;
    }

#ifdef USE_CGAL
    // Only proceed with CGAL processing if CGAL is available
    if (scene->mNumMeshes > 0) {
        // 2. Assimp mesh → CGAL mesh
        CGAL_Mesh cgal_mesh;
        if (assimp_mesh_to_cgal(scene->mMeshes[0], cgal_mesh)) {
            // 3. CGAL 修复与法线一致性
            repair_and_orient_cgal_mesh(cgal_mesh);

            // // 4. 顶面筛选与分割
            // auto top_faces = select_top_faces(cgal_mesh, 15.0);
            // auto cc_faces = split_connected_top_faces(cgal_mesh, top_faces);

            // // 5. 拷贝最大顶面
            // CGAL_Mesh top_surface;
            // extract_largest_top_surface(cgal_mesh, cc_faces, top_surface);

            // 6. CGAL mesh → Assimp mesh
            aiMesh* assimp_top_mesh = cgal_mesh_to_assimp(cgal_mesh);

            // 7. 构造新的 aiScene 返回
            aiScene* new_scene = new aiScene();
            new_scene->mNumMeshes = 1;
            new_scene->mMeshes = new aiMesh*[1];
            new_scene->mMeshes[0] = assimp_top_mesh;
            new_scene->mRootNode = new aiNode();
            new_scene->mRootNode->mNumMeshes = 1;
            new_scene->mRootNode->mMeshes = new unsigned int[1]{0};

            // 8. 返回 new_scene 给 ModelViewer
            return new_scene;
        }
    }
#endif

    // If CGAL is not available or processing failed, return the original scene
    return scene;
#else
    // If Assimp is not available
    std::cerr << "Error: Assimp library is not available. Cannot load model: " << filePath << std::endl;
    return nullptr;
#endif
}