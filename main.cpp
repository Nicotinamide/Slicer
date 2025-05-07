#include <iostream>

// 包含 Boost 头文件 (示例)
#include <boost/filesystem.hpp>
#include <boost/version.hpp>

// 包含 CGAL 头文件 (示例 - 根据你实际使用的CGAL组件来选择)
// 例如，一个简单的点
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Point_2.h>
#include <CGAL/version.h> // For CGAL_VERSION_STR

// 假设你有一个 slicer_core.h 在 src/ 目录
// #include "slicer_core.h"

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2 Point_2;

int main() {
    std::cout << "Hello from MySlicerProject!" << std::endl;

    // 使用 Boost Filesystem
    boost::filesystem::path current_path = boost::filesystem::current_path();
    std::cout << "Current path is: " << current_path.string() << std::endl;
    std::cout << "Using Boost version: " << BOOST_LIB_VERSION << std::endl;

    // 使用 CGAL
    Point_2 p1(0, 0), p2(1, 1);
    std::cout << "CGAL Point p1: " << p1 << std::endl;
    std::cout << "CGAL Point p2: " << p2 << std::endl;
    std::cout << "Using CGAL version: " << CGAL_VERSION_STR << " (Header), "
              << CGAL_VERSION_NR << " (Numeric)" << std::endl;


    // MySlicer slicer;
    // slicer.do_something();

    return 0;
}