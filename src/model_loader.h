// src/model_loader.h
#pragma once

#include <string>

#ifdef USE_ASSIMP
#include <assimp/scene.h>
#else
// Forward declaration for aiScene when Assimp is not available
struct aiScene;
#endif

// 加载模型并返回 aiScene 指针
// 如果Assimp不可用，返回nullptr
const aiScene* loadModelWithAssimp(const std::string& filePath);