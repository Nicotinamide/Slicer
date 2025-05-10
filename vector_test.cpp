#include <iostream>
#include <string>
#include <memory>
#include <cmath>
#include "src/model3d.h"

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

// Main function - just test Vec3 operations
int main() {
    std::cout << "Custom Vector Implementation Test" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // Test Vec3 operations
    testVec3Operations();
    
    return 0;
}
