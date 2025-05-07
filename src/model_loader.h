// src/model_loader.h
#pragma once

#include <string>
#include <assimp/scene.h>

// 加载模型并返回 aiScene 指针
const aiScene* loadModelWithAssimp(const std::string& filePath);