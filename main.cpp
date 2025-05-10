#include <iostream>
#include <filesystem>  // C++17 filesystem header

// 包含 Boost 头文件 (基本版本信息总是可用的)
#include <boost/version.hpp>

// 尝试包含 Boost filesystem，如果可用
#ifdef BOOST_FILESYSTEM_VERSION
#include <boost/filesystem.hpp>
#else
#ifdef __has_include
#if __has_include(<boost/filesystem.hpp>)
#include <boost/filesystem.hpp>
#define HAVE_BOOST_FILESYSTEM 1
#endif
#endif
#endif

// 包含 CGAL 头文件 (如果可用)
#ifdef USE_CGAL
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Point_2.h>
#include <CGAL/version.h> // For CGAL_VERSION_STR
#endif

// 假设你有一个 slicer_core.h 在 src/ 目录
// #include "slicer_core.h"

#ifdef USE_CGAL
typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2 Point_2;
#endif

int main() {
    std::cout << "Hello from MySlicerProject!" << std::endl;

    // 显示 Boost 版本
    std::cout << "Using Boost version: " << BOOST_LIB_VERSION << std::endl;    // 使用标准库获取当前路径
    std::cout << "Using standard library to get directory information:" << std::endl;
    try {
        // C++17 标准库的 filesystem
        std::cout << "Current path is: " << std::filesystem::current_path().string() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error getting current path: " << e.what() << std::endl;
    }
    
    // 使用 Boost Filesystem，如果可用
#if defined(HAVE_BOOST_FILESYSTEM) || defined(BOOST_FILESYSTEM_VERSION)
    std::cout << "Boost Filesystem is included." << std::endl;
    try {
        // 仅尝试创建一个简单的路径对象
        boost::filesystem::path test_path(".");
        std::cout << "Boost path successfully created." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Boost Filesystem error: " << e.what() << std::endl;
    }
#else
    std::cout << "Boost Filesystem component is not available in headers." << std::endl;
#endif

// 使用 CGAL (如果可用)
#ifdef USE_CGAL
    Point_2 p1(0, 0), p2(1, 1);
    std::cout << "CGAL Point p1: " << p1 << std::endl;
    std::cout << "CGAL Point p2: " << p2 << std::endl;
    std::cout << "Using CGAL version: " << CGAL_VERSION_STR << " (Header), "
              << CGAL_VERSION_NR << " (Numeric)" << std::endl;
#else
    std::cout << "CGAL functionality is not available in this build." << std::endl;
#endif

    // MySlicer slicer;
    // slicer.do_something();

    return 0;
}