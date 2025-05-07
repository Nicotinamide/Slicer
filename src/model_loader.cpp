// src/model_loader.cpp (或你的其他cpp文件)
#include "model_loader.h"
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// 使用静态导入器以保持场景生命周期
static Assimp::Importer importer;

const aiScene* loadModelWithAssimp(const std::string& filePath) {
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

    // 输出模型信息
    std::cout << "Successfully loaded model: " << filePath << std::endl;
    std::cout << "Number of meshes: " << scene->mNumMeshes << std::endl;
    if (scene->mNumMeshes > 0) {
        std::cout << "First mesh has " << scene->mMeshes[0]->mNumVertices << " vertices." << std::endl;
    }

    return scene;
}