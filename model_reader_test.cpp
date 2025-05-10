#include <iostream>
#include <string>
#include <memory>
#include <cmath>
#include <fstream>
#include "model3d.h"

// Function to test Vec3 operations
void testVec3Operations() {
    std::cout << "Testing Vec3 operations..." << std::endl;
    
    // Create test vectors
    Vec3 v1(1.0f, 2.0f, 3.0f);
    Vec3 v2(2.0f, 3.0f, 4.0f);
    
    // Test addition
    Vec3 vAdd = v1 + v2;
    std::cout << "Addition: (" << vAdd.x << ", " << vAdd.y << ", " << vAdd.z << ")" << std::endl;
    
    // Test subtraction
    Vec3 vSub = v1 - v2;
    std::cout << "Subtraction: (" << vSub.x << ", " << vSub.y << ", " << vSub.z << ")" << std::endl;
    
    // Test scalar multiplication
    Vec3 vMul = v1 * 2.0f;
    std::cout << "Scalar multiplication: (" << vMul.x << ", " << vMul.y << ", " << vMul.z << ")" << std::endl;
    
    // Test scalar division
    Vec3 vDiv = v1 / 2.0f;
    std::cout << "Scalar division: (" << vDiv.x << ", " << vDiv.y << ", " << vDiv.z << ")" << std::endl;
    
    // Test dot product
    float dotResult = v1.dot(v2);
    std::cout << "Dot product: " << dotResult << std::endl;
    
    // Test cross product
    Vec3 crossResult = v1.cross(v2);
    std::cout << "Cross product: (" << crossResult.x << ", " << crossResult.y << ", " << crossResult.z << ")" << std::endl;
    
    // Test length
    float len = v1.length();
    std::cout << "Length: " << len << std::endl;
    
    // Test normalize
    Vec3 vNorm = v1.normalize();
    std::cout << "Normalize: (" << vNorm.x << ", " << vNorm.y << ", " << vNorm.z << ")" << std::endl;
    std::cout << "Normalized length: " << vNorm.length() << std::endl;
    
    // Test distance
    float dist = Vec3::distance(v1, v2);
    std::cout << "Distance: " << dist << std::endl;
    
    // Test compound operators
    Vec3 vCompound = v1;
    vCompound += v2;
    std::cout << "Compound addition: (" << vCompound.x << ", " << vCompound.y << ", " << vCompound.z << ")" << std::endl;
    
    vCompound = v1;
    vCompound -= v2;
    std::cout << "Compound subtraction: (" << vCompound.x << ", " << vCompound.y << ", " << vCompound.z << ")" << std::endl;
    
    vCompound = v1;
    vCompound *= 2.0f;
    std::cout << "Compound multiplication: (" << vCompound.x << ", " << vCompound.y << ", " << vCompound.z << ")" << std::endl;
    
    vCompound = v1;
    vCompound /= 2.0f;
    std::cout << "Compound division: (" << vCompound.x << ", " << vCompound.y << ", " << vCompound.z << ")" << std::endl;
}

// Function to test model loading
void testModelLoading(const std::string& modelPath) {
    std::cout << "\nTesting model loading with file: " << modelPath << std::endl;

    Model3D model;
    bool success = model.loadModel(modelPath);

    if (!success) {
        std::cout << "Failed to load model!" << std::endl;
        return;
    }
    
    std::cout << "Model loaded successfully!" << std::endl;
    std::cout << "Model type: ";

    switch (model.getModelType()) {
        case ModelType::STL_ASCII:
            std::cout << "STL_ASCII";
            break;
        case ModelType::STL_BINARY:
            std::cout << "STL_BINARY";
            break;
        case ModelType::OBJ:
            std::cout << "OBJ";
            break;
        default:
            std::cout << "UNKNOWN";
    }
    std::cout << std::endl;
    
    // Print model info
    model.printModelInfo();
      // export to STL
    
    std::vector<Mesh> tmp_meshes, tmp_meshes2;
    
    tmp_meshes = model.extractSurfaces(5.0f);

    tmp_meshes2 = model.extractSurfacesByRegionGrowing(5.0f);

    model.printMeshStatistics(tmp_meshes);

    model.printMeshStatistics(tmp_meshes2);

    model.exportToSTL("exported_model.stl", tmp_meshes, false, true);
    model.exportToSTL("exported_model2.stl", tmp_meshes2, false, true);





    // 导出合并网格
    std::string exportMergedPath = "exported_merged_model.stl";
    if (model.exportToSTL(exportMergedPath, true)) {
        std::cout << "Merged mesh model exported successfully to: " << exportMergedPath << std::endl;
    } else {
        std::cout << "Failed to export merged mesh model!" << std::endl;
    }

    // Get and print bounding box
    Vec3 min, max;
    model.getBoundingBox(min, max);
    std::cout << "Bounding Box Min: (" << min.x << ", " << min.y << ", " << min.z << ")" << std::endl;
    std::cout << "Bounding Box Max: (" << max.x << ", " << max.y << ", " << max.z << ")" << std::endl;
    
    // Calculate and print dimensions
    Vec3 dimensions = max - min;
    std::cout << "Dimensions: (" << dimensions.x << ", " << dimensions.y << ", " << dimensions.z << ")" << std::endl;
    
    // Test triangle normal calculation
    if (!model.getMeshes().empty() && !model.getMeshes()[0].triangles.empty()) {
        const auto& mesh = model.getMeshes()[0];
        const auto& triangle = mesh.triangles[0];
        const auto& v0 = mesh.vertices[triangle.indices[0]].position;
        const auto& v1 = mesh.vertices[triangle.indices[1]].position;
        const auto& v2 = mesh.vertices[triangle.indices[2]].position;
        
        Vec3 calculatedNormal = calculateTriangleNormal(v0, v1, v2);
        std::cout << "Triangle Normal: (" << calculatedNormal.x << ", " 
                  << calculatedNormal.y << ", " << calculatedNormal.z << ")" << std::endl;
        std::cout << "Stored Normal: (" << triangle.normal.x << ", " 
                  << triangle.normal.y << ", " << triangle.normal.z << ")" << std::endl;
                  
        // Check if the normals are similar (allowing for floating point precision issues)
        float dotProd = calculatedNormal.dot(triangle.normal);
        std::cout << "Dot product of normals: " << dotProd << " (should be close to 1.0)" << std::endl;
    }
}

// Main function
int main(int argc, char* argv[]) {
    try {
        // 打开日志文件，记录程序执行情况
        std::ofstream logFile("test_log.txt");
        if (!logFile.is_open()) {
            std::cerr << "Failed to open log file!" << std::endl;
            return 1;
        }

        logFile << "Model Reader Test Program" << std::endl;
        logFile << "=======================" << std::endl;
        std::cout << "Model Reader Test Program" << std::endl;
        std::cout << "=======================" << std::endl;
        
        // Test Vec3 operations first
        logFile << "Starting Vec3 operations test..." << std::endl;
        testVec3Operations();
        logFile << "Vec3 operations test completed." << std::endl;
        
        // 先尝试输出一些中文文本以测试编码
        std::cout << std::endl << "中文测试文本 - Chinese Text Test" << std::endl;
        std::cout << "如果您能看到这段文字，说明UTF-8编码已正确设置" << std::endl;
        std::cout << "================================================" << std::endl << std::endl;
        
        // Determine file path
        std::string modelPath;
        if (argc > 1) {
            modelPath = argv[1]; // Use command line argument if provided
            logFile << "Using command line model path: " << modelPath << std::endl;
        } else {
            // Use default test model
            modelPath = "test_models/cube.stl";
            logFile << "Using default model path: " << modelPath << std::endl;
        }
        
        // Check if the file exists
        std::ifstream testFile(modelPath);
        if (!testFile) {
            logFile << "ERROR: Model file does not exist at path: " << modelPath << std::endl;
            // Try with an absolute path
            std::string absolutePath = "E:\\CodesE\\Slicer\\build\\bin\\Release\\test_models\\cube.stl";
            logFile << "Trying absolute path instead: " << absolutePath << std::endl;
            testFile = std::ifstream(absolutePath);
            if (testFile) {
                modelPath = absolutePath;
                logFile << "Absolute path exists, will use it instead." << std::endl;
            } else {
                logFile << "ERROR: Absolute path also does not exist!" << std::endl;
            }
        } else {
            logFile << "Model file exists at path: " << modelPath << std::endl;
        }
        
        // Test model loading
        logFile << "Starting model loading test..." << std::endl;
        testModelLoading(modelPath);
        logFile << "Model loading test completed." << std::endl;
        
        logFile.close();
        return 0;
    } catch (const std::exception& e) {
        std::ofstream errorLog("error_log.txt");
        errorLog << "Exception caught: " << e.what() << std::endl;
        errorLog.close();
        return 1;
    } catch (...) {
        std::ofstream errorLog("error_log.txt");
        errorLog << "Unknown exception caught!" << std::endl;
        errorLog.close();
        return 1;
    }
}
