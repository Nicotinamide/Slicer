#include "model_reader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <numeric>
#include <set>
#include <cctype>
#include <cstring>

// 辅助函数：去除字符串前后空白
static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// 辅助函数：转换字符串为小写
static inline std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

// 辅助函数：检查字符串是否以指定前缀开始
static inline bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && 
           std::equal(prefix.begin(), prefix.end(), str.begin());
}

// 辅助函数：更新包围盒
void updateBoundingBox(Vec3& min, Vec3& max, const Vec3& point) {
    min.x = (min.x < point.x) ? min.x : point.x;
    min.y = (min.y < point.y) ? min.y : point.y;
    min.z = (min.z < point.z) ? min.z : point.z;
    
    max.x = (max.x > point.x) ? max.x : point.x;
    max.y = (max.y > point.y) ? max.y : point.y;
    max.z = (max.z > point.z) ? max.z : point.z;
}

// 计算两点间距离
float distance(const Vec3& a, const Vec3& b) {
    return (a - b).length();
}

// 检测文件类型
ModelType ModelReader::detectFileType(const std::string& filePath) {
    // 首先通过扩展名判断
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    if (extension == ".stl") {
        // 尝试区分是ASCII还是二进制STL
        std::ifstream file(filePath, std::ios::binary);
        if (!file.good()) {
            return ModelType::UNKNOWN;
        }
        
        // 读取前6个字符，检查是否以"solid"开头
        char header[6] = {0};
        file.read(header, 5);
        
        if (std::strncmp(header, "solid", 5) == 0) {
            // 大多数ASCII STL以"solid"开头，但有些二进制STL也可能以"solid"开头
            // 为了更加确定，我们需要进一步检查
            file.seekg(0, std::ios::end);
            std::streamsize fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            
            // 如果文件大小精确地等于二进制STL的预期大小，很可能是二进制STL
            // 二进制STL结构：80字节头 + 4字节三角形数量 + 每个三角形50字节
            if ((fileSize - 84) % 50 == 0) {
                // 再次检查一下，从第80个字节开始读取4字节作为三角形数量
                file.seekg(80, std::ios::beg);
                uint32_t triangleCount;
                file.read(reinterpret_cast<char*>(&triangleCount), sizeof(triangleCount));
                
                // 测试文件大小是否匹配预期的三角形数量
                if (fileSize == 84 + (triangleCount * 50)) {
                    return ModelType::STL_BINARY;
                }
            }
            
            return ModelType::STL_ASCII;
        } 
        else {
            return ModelType::STL_BINARY;
        }
    } 
    else if (extension == ".obj") {
        return ModelType::OBJ;
    }
    
    return ModelType::UNKNOWN;
}

// 加载模型
bool ModelReader::loadModel(const std::string& filePath) {
    // 清除现有数据
    clear();
    
    // 设置文件目录
    m_directory = std::filesystem::path(filePath).parent_path().string();
    
    // 检测文件类型
    m_modelType = detectFileType(filePath);
    
    // 根据文件类型调用相应的读取函数
    bool result = false;
    switch (m_modelType) {
        case ModelType::STL_ASCII:
            result = readSTLAscii(filePath);
            break;
        case ModelType::STL_BINARY:
            result = readSTLBinary(filePath);
            break;
        case ModelType::OBJ:
            result = readOBJ(filePath);
            break;
        default:
            std::cerr << "不支持的文件格式: " << filePath << std::endl;
            return false;
    }
    
    // 如果读取成功，计算模型中心点
    if (result && !m_meshes.empty()) {
        for (auto& mesh : m_meshes) {
            Vec3 center(0.0f, 0.0f, 0.0f);
            for (const auto& vertex : mesh.vertices) {
                center = center + vertex.position;
            }
            
            if (!mesh.vertices.empty()) {
                center = center / static_cast<float>(mesh.vertices.size());
            }
            
            mesh.center = center;
        }
    }
    
    return result;
}

// 读取ASCII STL文件
bool ModelReader::readSTLAscii(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.good()) {
        std::cerr << "无法打开STL文件: " << filePath << std::endl;
        return false;
    }
    
    std::string line;
    std::string solidName;
    
    // 读取solid行，获取名称
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        
        if (toLower(token) != "solid") {
            std::cerr << "STL文件格式错误: 缺少'solid'关键字" << std::endl;
            return false;
        }
        
        // 提取solid名称
        solidName = line.substr(5);
        solidName = trim(solidName);
    }
    
    // 创建新的网格
    Mesh mesh;
    mesh.name = solidName.empty() ? "unnamed_stl" : solidName;
    
    Vec3 normal(0.0f, 0.0f, 0.0f);
    bool inFacet = false;
    bool inLoop = false;
    int vertexCount = 0;
    Triangle currentTriangle;
    
    // 逐行解析文件
    while (std::getline(file, line)) {
        std::istringstream iss(trim(line));
        std::string token;
        iss >> token;
        token = toLower(token);
        
        if (token == "facet") {
            if (inFacet) {
                std::cerr << "STL格式错误: 嵌套的facet" << std::endl;
                return false;
            }
            
            inFacet = true;
            vertexCount = 0;
            
            // 读取法线
            std::string normalToken;
            iss >> normalToken;
            
            if (toLower(normalToken) == "normal") {
                iss >> normal.x >> normal.y >> normal.z;
                currentTriangle.normal = normal;
            } else {
                std::cerr << "STL格式错误: 'facet'后缺少'normal'" << std::endl;
                return false;
            }
        } 
        else if (token == "outer") {
            if (!inFacet || inLoop) {
                std::cerr << "STL格式错误: 'outer'关键字位置不正确" << std::endl;
                return false;
            }
            
            std::string loopToken;
            iss >> loopToken;
            
            if (toLower(loopToken) != "loop") {
                std::cerr << "STL格式错误: 'outer'后缺少'loop'" << std::endl;
                return false;
            }
            
            inLoop = true;
        } 
        else if (token == "vertex") {
            if (!inLoop) {
                std::cerr << "STL格式错误: 'vertex'关键字在loop外部" << std::endl;
                return false;
            }
            
            if (vertexCount >= 3) {
                std::cerr << "STL格式错误: 每个facet超过3个顶点" << std::endl;
                return false;
            }
            
            Vertex v;
            iss >> v.position.x >> v.position.y >> v.position.z;
            v.normal = normal; // 使用面法线作为顶点法线
            v.color = Vec3(0.8f, 0.8f, 0.8f); // 默认颜色
            
            // 更新包围盒
            updateBoundingBox(m_boundingBoxMin, m_boundingBoxMax, v.position);
            
            // 添加顶点到网格
            mesh.vertices.push_back(v);
            
            // 记录顶点索引
            currentTriangle.indices[vertexCount] = mesh.vertices.size() - 1;
            vertexCount++;
        } 
        else if (token == "endloop") {
            if (!inLoop) {
                std::cerr << "STL格式错误: 'endloop'关键字无对应的'loop'" << std::endl;
                return false;
            }
            
            inLoop = false;
        } 
        else if (token == "endfacet") {
            if (!inFacet) {
                std::cerr << "STL格式错误: 'endfacet'关键字无对应的'facet'" << std::endl;
                return false;
            }
            
            inFacet = false;
            
            // 添加当前三角形
            if (vertexCount == 3) {
                mesh.triangles.push_back(currentTriangle);
                
                // 添加索引到索引数组
                mesh.indices.push_back(currentTriangle.indices[0]);
                mesh.indices.push_back(currentTriangle.indices[1]);
                mesh.indices.push_back(currentTriangle.indices[2]);
            } else {
                std::cerr << "STL格式错误: facet未包含3个顶点" << std::endl;
                return false;
            }
        } 
        else if (token == "endsolid") {
            // 存储完成的网格
            m_meshes.push_back(mesh);
            return true;
        }
    }
    
    // 如果没有正确读到"endsolid"，文件可能不完整
    std::cerr << "STL格式错误: 缺少'endsolid'关键字" << std::endl;
    return false;
}

// 读取二进制STL文件
bool ModelReader::readSTLBinary(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.good()) {
        std::cerr << "无法打开STL文件: " << filePath << std::endl;
        return false;
    }
    
    // 创建新网格
    Mesh mesh;
    mesh.name = std::filesystem::path(filePath).stem().string(); // 使用文件名作为网格名称
    
    // 读取80字节的头部信息(通常不使用)
    char header[80] = {0};
    file.read(header, 80);
    
    // 头部可能包含solid名称，尝试提取(这是个启发式方法，并非标准)
    header[79] = '\0'; // 确保以null结尾
    if (std::strlen(header) > 0) {
        mesh.name = header;
    }
    
    // 读取三角形数量
    uint32_t triangleCount = 0;
    file.read(reinterpret_cast<char*>(&triangleCount), sizeof(triangleCount));
    
    if (triangleCount == 0) {
        std::cerr << "STL文件不包含任何三角形" << std::endl;
        return false;
    }
    
    // 预留空间
    mesh.vertices.reserve(triangleCount * 3); // 最坏情况：每个三角形3个唯一顶点
    mesh.triangles.reserve(triangleCount);
    mesh.indices.reserve(triangleCount * 3);
    
    // 读取所有三角形
    for (uint32_t i = 0; i < triangleCount; ++i) {
        // 二进制STL三角形结构：
        // - 法线(3个float): 12字节
        // - 3个顶点(每个3个float): 36字节
        // - 2字节属性(通常不使用)
        // 总共: 50字节/三角形
        
        // 读取法线
        Vec3 normal;
        file.read(reinterpret_cast<char*>(&normal.x), sizeof(float) * 3);
        
        Triangle triangle;
        triangle.normal = normal;
        
        // 读取3个顶点
        for (int j = 0; j < 3; ++j) {
            Vertex vertex;
            file.read(reinterpret_cast<char*>(&vertex.position.x), sizeof(float) * 3);
            
            vertex.normal = normal; // 使用面法线
            vertex.color = Vec3(0.8f, 0.8f, 0.8f); // 默认颜色
            
            // 更新包围盒
            updateBoundingBox(m_boundingBoxMin, m_boundingBoxMax, vertex.position);
            
            // 添加顶点
            mesh.vertices.push_back(vertex);
            
            // 记录顶点索引
            triangle.indices[j] = mesh.vertices.size() - 1;
            
            // 添加到索引数组
            mesh.indices.push_back(triangle.indices[j]);
        }
        
        // 读取2字节属性值(通常不使用)
        uint16_t attributeByteCount;
        file.read(reinterpret_cast<char*>(&attributeByteCount), sizeof(attributeByteCount));
        
        // 添加三角形到网格
        mesh.triangles.push_back(triangle);
    }
    
    // 检查是否读取完整
    if (!file.eof() && !file.good()) {
        std::cerr << "读取二进制STL文件时出错" << std::endl;
        return false;
    }
    
    // 优化网格(合并重复顶点)
    optimizeMesh(mesh);
    
    // 添加网格到集合
    m_meshes.push_back(mesh);
    
    return true;
}

// 读取OBJ文件
bool ModelReader::readOBJ(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.good()) {
        std::cerr << "无法打开OBJ文件: " << filePath << std::endl;
        return false;
    }
    
    // 创建默认网格
    Mesh currentMesh;
    currentMesh.name = std::filesystem::path(filePath).stem().string();
    
    // 临时存储所有顶点、法线和纹理坐标数据
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texCoords;
    
    // 当前材质名称
    std::string currentMaterialName;
    
    std::string line;
    while (std::getline(file, line)) {
        // 跳过空行和注释
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        
        if (token == "v") {
            // 顶点位置
            Vec3 position;
            iss >> position.x >> position.y >> position.z;
            
            // 更新包围盒
            updateBoundingBox(m_boundingBoxMin, m_boundingBoxMax, position);
            
            positions.push_back(position);
        }
        else if (token == "vn") {
            // 顶点法线
            Vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal.normalize());
        }
        else if (token == "vt") {
            // 纹理坐标
            Vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            texCoords.push_back(texCoord);
        }
        else if (token == "f") {
            // 面数据
            std::vector<int> posIndices;
            std::vector<int> texIndices;
            std::vector<int> normalIndices;
            
            std::string vertex;
            // 解析面的每个顶点
            while (iss >> vertex) {
                std::istringstream viss(vertex);
                std::string indexStr;
                
                // 格式可能是：v、v/vt、v/vt/vn 或 v//vn
                // v: 顶点索引
                // vt: 纹理坐标索引
                // vn: 法线索引
                
                // 顶点位置索引
                std::getline(viss, indexStr, '/');
                if (!indexStr.empty()) {
                    int index = std::stoi(indexStr);
                    // OBJ索引从1开始，转换为从0开始
                    index = (index > 0) ? index - 1 : positions.size() + index;
                    posIndices.push_back(index);
                }
                
                // 纹理坐标索引
                std::getline(viss, indexStr, '/');
                if (!indexStr.empty()) {
                    int index = std::stoi(indexStr);
                    index = (index > 0) ? index - 1 : texCoords.size() + index;
                    texIndices.push_back(index);
                }
                
                // 法线索引
                std::getline(viss, indexStr);
                if (!indexStr.empty()) {
                    int index = std::stoi(indexStr);
                    index = (index > 0) ? index - 1 : normals.size() + index;
                    normalIndices.push_back(index);
                }
            }
            
            // 确认是否有足够的顶点构成一个多边形
            if (posIndices.size() >= 3) {
                // 添加顶点到当前网格
                for (size_t i = 0; i < posIndices.size(); ++i) {
                    Vertex v;
                    v.position = positions[posIndices[i]];
                    
                    // 如果有法线数据
                    if (i < normalIndices.size() && normalIndices[i] >= 0 && normalIndices[i] < normals.size()) {
                        v.normal = normals[normalIndices[i]];
                    }
                    
                    // 如果有纹理坐标数据
                    if (i < texIndices.size() && texIndices[i] >= 0 && texIndices[i] < texCoords.size()) {
                        v.texCoord = texCoords[texIndices[i]];
                    }
                    
                    v.color = Vec3(0.8f, 0.8f, 0.8f); // 默认颜色
                    
                    // 添加到网格
                    currentMesh.vertices.push_back(v);
                }
                
                // 三角形化多边形(面可能是三角形、四边形或更多边的多边形)
                for (size_t i = 2; i < posIndices.size(); ++i) {
                    Triangle triangle;
                    triangle.indices[0] = currentMesh.vertices.size() - posIndices.size();
                    triangle.indices[1] = currentMesh.vertices.size() - posIndices.size() + i - 1;
                    triangle.indices[2] = currentMesh.vertices.size() - posIndices.size() + i;
                    
                    // 计算面法线
                    const auto& v0 = currentMesh.vertices[triangle.indices[0]].position;
                    const auto& v1 = currentMesh.vertices[triangle.indices[1]].position;
                    const auto& v2 = currentMesh.vertices[triangle.indices[2]].position;
                    triangle.normal = calculateTriangleNormal(v0, v1, v2);
                    
                    // 添加三角形
                    currentMesh.triangles.push_back(triangle);
                    
                    // 添加到索引数组
                    currentMesh.indices.push_back(triangle.indices[0]);
                    currentMesh.indices.push_back(triangle.indices[1]);
                    currentMesh.indices.push_back(triangle.indices[2]);
                }
            }
        }
        else if (token == "mtllib") {
            // 材质库
            std::string mtlFileName;
            iss >> mtlFileName;
            
            // 读取材质文件
            std::string mtlFilePath = (std::filesystem::path(filePath).parent_path() / mtlFileName).string();
            readMTL(mtlFilePath);
        }
        else if (token == "usemtl") {
            // 使用材质
            std::string materialName;
            iss >> materialName;
            
            // 如果材质变化了，可能需要创建新网格
            // 这里简化处理，只修改当前网格的材质名称
            if (!materialName.empty() && materialName != currentMaterialName) {
                currentMaterialName = materialName;
                
                // 设置材质
                if (m_materials.find(materialName) != m_materials.end()) {
                    currentMesh.material = m_materials[materialName];
                }
            }
        }
        else if (token == "o" || token == "g") {
            // 对象或组名称
            std::string name;
            std::getline(iss, name);
            name = trim(name);
            
            // 如果当前网格已有数据，保存它并创建新网格
            if (!currentMesh.vertices.empty() && !currentMesh.indices.empty()) {
                // 优化当前网格
                optimizeMesh(currentMesh);
                
                // 保存当前网格
                m_meshes.push_back(currentMesh);
                
                // 创建新网格
                currentMesh = Mesh();
                currentMesh.name = name;
                
                // 保持当前材质
                if (!currentMaterialName.empty() && m_materials.find(currentMaterialName) != m_materials.end()) {
                    currentMesh.material = m_materials[currentMaterialName];
                }
            } 
            else {
                // 仅更新网格名称
                currentMesh.name = name;
            }
        }
    }



    // 保存最后一个网格
    if (!currentMesh.vertices.empty() && !currentMesh.indices.empty()) {
        // 优化当前网格
        optimizeMesh(currentMesh);
        
        // 保存当前网格
        m_meshes.push_back(currentMesh);
    }
    
    // 如果没有读取到任何网格，返回失败
    if (m_meshes.empty()) {
        std::cerr << "OBJ文件不包含有效网格数据" << std::endl;
        return false;
    }
    
    return true;
}

// 读取MTL材质文件
bool ModelReader::readMTL(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.good()) {
        std::cerr << "无法打开MTL文件: " << filePath << std::endl;
        return false;
    }
    
    Material* currentMaterial = nullptr;
    
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        
        if (token == "newmtl") {
            // 新材质
            std::string name;
            std::getline(iss, name);
            name = trim(name);
            
            if (!name.empty()) {
                m_materials[name] = Material();
                currentMaterial = &m_materials[name];
                currentMaterial->name = name;
            }
        }
        else if (token == "Ka" && currentMaterial) {
            // 环境光颜色
            iss >> currentMaterial->ambient.r >> currentMaterial->ambient.g >> currentMaterial->ambient.b;
        }
        else if (token == "Kd" && currentMaterial) {
            // 漫反射颜色
            iss >> currentMaterial->diffuse.r >> currentMaterial->diffuse.g >> currentMaterial->diffuse.b;
        }
        else if (token == "Ks" && currentMaterial) {
            // 镜面反射颜色
            iss >> currentMaterial->specular.r >> currentMaterial->specular.g >> currentMaterial->specular.b;
        }
        else if (token == "Ns" && currentMaterial) {
            // 镜面反射指数
            iss >> currentMaterial->shininess;
        }
        else if (token == "map_Kd" && currentMaterial) {
            // 漫反射贴图
            std::string texPath;
            std::getline(iss, texPath);
            texPath = trim(texPath);
            
            if (!texPath.empty()) {
                // 尝试解析贴图路径
                std::filesystem::path mapPath(texPath);
                std::string mapFilename = mapPath.filename().string();
                
                // 在模型目录下查找贴图
                std::string fullPath = m_directory + "/" + mapFilename;
                
                if (std::filesystem::exists(fullPath)) {
                    currentMaterial->diffuseMap = fullPath;
                } else {
                    currentMaterial->diffuseMap = texPath; // 使用原始路径
                }
            }
        }
        else if (token == "map_Bump" && currentMaterial) {
            // 法线贴图
            std::string texPath;
            std::getline(iss, texPath);
            texPath = trim(texPath);
            
            if (!texPath.empty()) {
                std::filesystem::path mapPath(texPath);
                std::string mapFilename = mapPath.filename().string();
                
                std::string fullPath = m_directory + "/" + mapFilename;
                
                if (std::filesystem::exists(fullPath)) {
                    currentMaterial->normalMap = fullPath;
                } else {
                    currentMaterial->normalMap = texPath;
                }
            }
        }
    }
    
    return true;
}

// 优化网格(合并重复顶点)
void ModelReader::optimizeMesh(Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.indices.empty()) return;
    
    // 创建顶点到新索引的映射
    std::unordered_map<size_t, size_t> uniqueVertices;
    std::vector<Vertex> optimizedVertices;
    std::vector<unsigned int> optimizedIndices;
    
    // 简单的顶点哈希函数
    auto vertexHash = [](const Vertex& v) -> size_t {
        // 使用位置作为哈希
        const float epsilon = 0.00001f;
        int x = static_cast<int>(v.position.x / epsilon);
        int y = static_cast<int>(v.position.y / epsilon);
        int z = static_cast<int>(v.position.z / epsilon);
        
        return std::hash<int>()(x) ^ 
               (std::hash<int>()(y) << 1) ^ 
               (std::hash<int>()(z) << 2);
    };
    
    // 比较两个顶点是否相同
    auto vertexEqual = [](const Vertex& a, const Vertex& b) -> bool {
        const float epsilon = 0.00001f;
        return Vec3::distance(a.position, b.position) < epsilon;
    };
    
    // 处理每个三角形
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        for (size_t j = 0; j < 3; ++j) {
            const Vertex& v = mesh.vertices[mesh.indices[i + j]];
            size_t hash = vertexHash(v);
            
            bool found = false;
            for (auto it = uniqueVertices.find(hash); 
                 it != uniqueVertices.end() && it->first == hash; ++it) {
                if (vertexEqual(v, optimizedVertices[it->second])) {
                    optimizedIndices.push_back(it->second);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                optimizedIndices.push_back(optimizedVertices.size());
                uniqueVertices[hash] = optimizedVertices.size();
                optimizedVertices.push_back(v);
            }
        }
    }
    
    // 更新网格数据
    if (optimizedVertices.size() < mesh.vertices.size()) {
        mesh.vertices = std::move(optimizedVertices);
        mesh.indices = std::move(optimizedIndices);
        
        // 更新三角形索引
        mesh.triangles.clear();
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            Triangle triangle;
            triangle.indices[0] = mesh.indices[i];
            triangle.indices[1] = mesh.indices[i + 1];
            triangle.indices[2] = mesh.indices[i + 2];
            
            // 计算面法线
            const auto& v0 = mesh.vertices[triangle.indices[0]].position;
            const auto& v1 = mesh.vertices[triangle.indices[1]].position;
            const auto& v2 = mesh.vertices[triangle.indices[2]].position;
            triangle.normal = calculateTriangleNormal(v0, v1, v2);
            
            mesh.triangles.push_back(triangle);
        }
    }
    
    // 计算顶点法线(如果需要)
    calculateNormals(mesh);
}

// 计算法线
void ModelReader::calculateNormals(Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.triangles.empty()) return;
    
    // 重置所有顶点的法线
    for (auto& v : mesh.vertices) {
        v.normal = Vec3(0.0f, 0.0f, 0.0f);
    }
    
    // 累加每个面的法线到其顶点
    for (const auto& tri : mesh.triangles) {
        // 应用面法线到三个顶点
        for (int i = 0; i < 3; ++i) {
            mesh.vertices[tri.indices[i]].normal += tri.normal;
        }
    }
    
    // 归一化所有顶点法线
    for (auto& v : mesh.vertices) {
        if (v.normal.length() > 0.00001f) {
            v.normal = v.normal.normalize();
        } else {
            v.normal = Vec3(0.0f, 0.0f, 1.0f); // 默认法线
        }
    }
}

// 获取包围盒
void ModelReader::getBoundingBox(Vec3& min, Vec3& max) const {
    min = m_boundingBoxMin;
    max = m_boundingBoxMax;
}

// 打印模型信息
void ModelReader::printModelInfo() const {
    std::cout << "======== 模型信息 ========" << std::endl;
    
    std::string modelTypeStr;
    switch (m_modelType) {
        case ModelType::STL_ASCII: modelTypeStr = "STL (ASCII)"; break;
        case ModelType::STL_BINARY: modelTypeStr = "STL (二进制)"; break;
        case ModelType::OBJ: modelTypeStr = "OBJ"; break;
        default: modelTypeStr = "未知"; break;
    }
    
    std::cout << "文件类型: " << modelTypeStr << std::endl;
    std::cout << "网格数量: " << m_meshes.size() << std::endl;
    std::cout << "材质数量: " << m_materials.size() << std::endl;
    
    int totalVertices = 0;
    int totalTriangles = 0;
    
    for (const auto& mesh : m_meshes) {
        totalVertices += mesh.vertices.size();
        totalTriangles += mesh.triangles.size();
    }
    
    std::cout << "总顶点数: " << totalVertices << std::endl;
    std::cout << "总三角形数: " << totalTriangles << std::endl;
    
    std::cout << "包围盒: " << std::endl;
    std::cout << "  最小点: (" << m_boundingBoxMin.x << ", " << m_boundingBoxMin.y << ", " << m_boundingBoxMin.z << ")" << std::endl;
    std::cout << "  最大点: (" << m_boundingBoxMax.x << ", " << m_boundingBoxMax.y << ", " << m_boundingBoxMax.z << ")" << std::endl;
    
    // 模型大小
    Vec3 size = m_boundingBoxMax - m_boundingBoxMin;
    std::cout << "  尺寸: (" << size.x << ", " << size.y << ", " << size.z << ")" << std::endl;
    
    // 打印每个网格的信息
    if (!m_meshes.empty()) {
        std::cout << std::endl << "网格详情:" << std::endl;
        for (size_t i = 0; i < m_meshes.size(); ++i) {
            const auto& mesh = m_meshes[i];
            std::cout << "  [" << i << "] " << mesh.name << ": " 
                      << mesh.vertices.size() << " 顶点, " 
                      << mesh.triangles.size() << " 三角形" << std::endl;
        }
    }
    
    std::cout << "=========================" << std::endl;
}

// 导出为STL文件
bool ModelReader::exportToSTL(const std::string& filePath, bool binary, bool mergeMeshes) const {
    if (m_meshes.empty()) {
        std::cerr << "没有要导出的网格数据" << std::endl;
        return false;
    }

    // 如果只有一个网格，忽略mergeMeshes参数
    if (m_meshes.size() == 1) {
        mergeMeshes = false;

    }
    
    // 合并模式：所有网格合并为一个STL文件
    if (mergeMeshes) {
        if (binary) {
            // 二进制STL
            std::ofstream file(filePath, std::ios::binary);
            if (!file.good()) {
                std::cerr << "无法创建STL文件: " << filePath << std::endl;
                return false;
            }
            
            // 写入80字节的头部信息
            char header[80] = {0};
            std::snprintf(header, 80, "STL exported by ModelReader - Merged Meshes");
            file.write(header, 80);
            
            // 计算总三角形数量
            uint32_t totalTriangleCount = 0;
            for (const auto& mesh : m_meshes) {
                totalTriangleCount += static_cast<uint32_t>(mesh.triangles.size());
            }
            
            // 写入三角形数量
            file.write(reinterpret_cast<const char*>(&totalTriangleCount), sizeof(totalTriangleCount));
            
            // 写入所有网格的三角形
            for (const auto& mesh : m_meshes) {
                for (const auto& tri : mesh.triangles) {
                    // 写入法线
                    file.write(reinterpret_cast<const char*>(&tri.normal), sizeof(float) * 3);
                    
                    // 写入三个顶点
                    for (int i = 0; i < 3; ++i) {
                        const auto& pos = mesh.vertices[tri.indices[i]].position;
                        file.write(reinterpret_cast<const char*>(&pos), sizeof(float) * 3);
                    }
                    
                    // 写入2字节属性值(通常设为0)
                    uint16_t attributeByteCount = 0;
                    file.write(reinterpret_cast<const char*>(&attributeByteCount), sizeof(attributeByteCount));
                }
            }
        } else {
            // ASCII STL
            std::ofstream file(filePath);
            if (!file.good()) {
                std::cerr << "无法创建STL文件: " << filePath << std::endl;
                return false;
            }
            
            // 写入头部
            file << "solid MergedModel" << std::endl;
            
            // 导出所有网格的三角形
            for (const auto& mesh : m_meshes) {
                for (const auto& tri : mesh.triangles) {
                    file << "  facet normal " 
                         << tri.normal.x << " " << tri.normal.y << " " << tri.normal.z << std::endl;
                    file << "    outer loop" << std::endl;
                    
                    // 写入三个顶点
                    for (int i = 0; i < 3; ++i) {
                        const auto& pos = mesh.vertices[tri.indices[i]].position;
                        file << "      vertex " << pos.x << " " << pos.y << " " << pos.z << std::endl;
                    }
                    
                    file << "    endloop" << std::endl;
                    file << "  endfacet" << std::endl;
                }
            }
            
            // 写入结尾
            file << "endsolid MergedModel" << std::endl;
        }
        
        return true;
    }
    // 分组模式：每个网格导出为单独的文件
    else  {
        // 获取文件名和扩展名
        std::filesystem::path path(filePath);
        std::string baseName = path.stem().string();
        std::string extension = path.extension().string();
        std::string directory = path.parent_path().string();
        
        // 记录是否所有导出都成功
        bool allSuccess = true;
        
        // 为每个网格创建单独的文件
        for (size_t i = 0; i < m_meshes.size(); ++i) {
            const auto& mesh = m_meshes[i];
            
            // 构建文件名：baseName_i.stl
            std::string meshFilename;
            if (directory.empty()) {
                meshFilename = baseName + "_" + std::to_string(i) + extension;
            } else {
                meshFilename = directory + "/" + baseName + "_" + std::to_string(i) + extension;
            }
            
            if (binary) {
                // 二进制STL
                std::ofstream file(meshFilename, std::ios::binary);
                if (!file.good()) {
                    std::cerr << "无法创建STL文件: " << meshFilename << std::endl;
                    allSuccess = false;
                    continue;
                }
                
                // 写入80字节的头部信息
                char header[80] = {0};
                std::snprintf(header, 80, "STL exported by ModelReader - %s", mesh.name.c_str());
                file.write(header, 80);
                
                // 写入三角形数量
                uint32_t triangleCount = static_cast<uint32_t>(mesh.triangles.size());
                file.write(reinterpret_cast<const char*>(&triangleCount), sizeof(triangleCount));
                
                // 写入三角形数据
                for (const auto& tri : mesh.triangles) {
                    // 写入法线
                    file.write(reinterpret_cast<const char*>(&tri.normal), sizeof(float) * 3);
                    
                    // 写入三个顶点
                    for (int j = 0; j < 3; ++j) {
                        const auto& pos = mesh.vertices[tri.indices[j]].position;
                        file.write(reinterpret_cast<const char*>(&pos), sizeof(float) * 3);
                    }
                    
                    // 写入2字节属性值(通常设为0)
                    uint16_t attributeByteCount = 0;
                    file.write(reinterpret_cast<const char*>(&attributeByteCount), sizeof(attributeByteCount));
                }
            } else {
                // ASCII STL
                std::ofstream file(meshFilename);
                if (!file.good()) {
                    std::cerr << "无法创建STL文件: " << meshFilename << std::endl;
                    allSuccess = false;
                    continue;
                }
                
                // 写入头部
                file << "solid " << mesh.name << std::endl;
                
                // 写入三角形数据
                for (const auto& tri : mesh.triangles) {
                    file << "  facet normal " 
                         << tri.normal.x << " " << tri.normal.y << " " << tri.normal.z << std::endl;
                    file << "    outer loop" << std::endl;
                    
                    // 写入三个顶点
                    for (int j = 0; j < 3; ++j) {
                        const auto& pos = mesh.vertices[tri.indices[j]].position;
                        file << "      vertex " << pos.x << " " << pos.y << " " << pos.z << std::endl;
                    }
                    
                    file << "    endloop" << std::endl;
                    file << "  endfacet" << std::endl;
                }
                
                // 写入结尾
                file << "endsolid " << mesh.name << std::endl;
            }
            
            std::cout << "网格 " << i << " (" << mesh.name << ") 已导出到: " << meshFilename << std::endl;
        }
        
        return allSuccess;
    }
    
}

// 导出为OBJ文件
bool ModelReader::exportToOBJ(const std::string& filePath) const {
    if (m_meshes.empty()) {
        std::cerr << "没有要导出的网格数据" << std::endl;
        return false;
    }
    
    std::ofstream objFile(filePath);
    if (!objFile.good()) {
        std::cerr << "无法创建OBJ文件: " << filePath << std::endl;
        return false;
    }
    
    // 写入OBJ文件头部注释
    objFile << "# OBJ file exported by ModelReader" << std::endl;
    objFile << "# Meshes: " << m_meshes.size() << std::endl;
    objFile << "# Total vertices: " << std::accumulate(m_meshes.begin(), m_meshes.end(), 0,
        [](int sum, const Mesh& mesh) { return sum + mesh.vertices.size(); }) << std::endl;
    objFile << "# Total triangles: " << std::accumulate(m_meshes.begin(), m_meshes.end(), 0,
        [](int sum, const Mesh& mesh) { return sum + mesh.triangles.size(); }) << std::endl;
    objFile << std::endl;
    
    // 如果有材质，创建MTL文件
    std::string mtlFilename = std::filesystem::path(filePath).stem().string() + ".mtl";
    std::string mtlFullPath = (std::filesystem::path(filePath).parent_path() / mtlFilename).string();
    
    // 材质集合
    std::set<std::string> usedMaterials;
    for (const auto& mesh : m_meshes) {
        if (!mesh.material.name.empty()) {
            usedMaterials.insert(mesh.material.name);
        }
    }
    
    // 如果有材质，写入MTL文件引用
    if (!usedMaterials.empty()) {
        objFile << "mtllib " << mtlFilename << std::endl << std::endl;
        
        // 创建MTL文件
        std::ofstream mtlFile(mtlFullPath);
        if (mtlFile.good()) {
            mtlFile << "# MTL file exported by ModelReader" << std::endl << std::endl;
            
            // 遍历所有网格的材质
            for (const auto& mesh : m_meshes) {
                if (mesh.material.name.empty()) continue;
                
                mtlFile << "newmtl " << mesh.material.name << std::endl;
                mtlFile << "Ka " << mesh.material.ambient.r << " " 
                        << mesh.material.ambient.g << " " 
                        << mesh.material.ambient.b << std::endl;
                mtlFile << "Kd " << mesh.material.diffuse.r << " " 
                        << mesh.material.diffuse.g << " " 
                        << mesh.material.diffuse.b << std::endl;
                mtlFile << "Ks " << mesh.material.specular.r << " " 
                        << mesh.material.specular.g << " " 
                        << mesh.material.specular.b << std::endl;
                mtlFile << "Ns " << mesh.material.shininess << std::endl;
                
                if (!mesh.material.diffuseMap.empty()) {
                    mtlFile << "map_Kd " << std::filesystem::path(mesh.material.diffuseMap).filename().string() << std::endl;
                }
                
                if (!mesh.material.normalMap.empty()) {
                    mtlFile << "map_Bump " << std::filesystem::path(mesh.material.normalMap).filename().string() << std::endl;
                }
                
                mtlFile << std::endl;
            }
        } else {
            std::cerr << "警告: 无法创建MTL文件: " << mtlFullPath << std::endl;
        }
    }
    
    // 用于跟踪顶点索引的偏移量
    int vertexOffset = 1;  // OBJ索引从1开始
    int normalOffset = 1;
    int texCoordOffset = 1;
    
    // 处理每个网格
    for (const auto& mesh : m_meshes) {
        // 写入对象名称
        objFile << "o " << mesh.name << std::endl;
        
        // 写入材质引用
        if (!mesh.material.name.empty()) {
            objFile << "usemtl " << mesh.material.name << std::endl;
        }
        
        // 写入顶点位置
        for (const auto& vertex : mesh.vertices) {
            objFile << "v " << vertex.position.x << " " 
                   << vertex.position.y << " " 
                   << vertex.position.z << std::endl;
        }
        
        // 写入纹理坐标
        bool hasTexCoords = false;
        for (const auto& vertex : mesh.vertices) {
            if (vertex.texCoord.x != 0.0f || vertex.texCoord.y != 0.0f) {
                hasTexCoords = true;
                break;
            }
        }
        
        if (hasTexCoords) {
            for (const auto& vertex : mesh.vertices) {
                objFile << "vt " << vertex.texCoord.x << " " 
                       << vertex.texCoord.y << std::endl;
            }
        }
        
        // 写入法线
        for (const auto& vertex : mesh.vertices) {
            objFile << "vn " << vertex.normal.x << " " 
                   << vertex.normal.y << " " 
                   << vertex.normal.z << std::endl;
        }
        
        objFile << std::endl;
        
        // 写入面
        for (const auto& tri : mesh.triangles) {
            objFile << "f ";
            
            for (int i = 0; i < 3; ++i) {
                int vertexIdx = vertexOffset + tri.indices[i];
                int texCoordIdx = hasTexCoords ? texCoordOffset + tri.indices[i] : 0;
                int normalIdx = normalOffset + tri.indices[i];
                
                if (hasTexCoords) {
                    objFile << vertexIdx << "/" << texCoordIdx << "/" << normalIdx;
                } else {
                    objFile << vertexIdx << "//" << normalIdx;
                }
                
                if (i < 2) objFile << " ";
            }
            
            objFile << std::endl;
        }
        
        // 更新偏移量
        vertexOffset += mesh.vertices.size();
        if (hasTexCoords) texCoordOffset += mesh.vertices.size();
        normalOffset += mesh.vertices.size();
        
        objFile << std::endl;
    }
    
    return true;
}

// 清除数据
void ModelReader::clear() {
    m_meshes.clear();
    m_materials.clear();
    m_modelType = ModelType::UNKNOWN;
    
    // 避免使用std::numeric_limits<float>::max()，因为和Windows的max宏冲突
    float maxval = 3.402823466e+38F;  // float的最大值
    m_boundingBoxMin = Vec3(maxval, maxval, maxval);
    m_boundingBoxMax = Vec3(-maxval, -maxval, -maxval);
}
