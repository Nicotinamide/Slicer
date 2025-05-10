# E:/CodesE/Slicer/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(MySlicerProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加构建选项，控制是否构建GUI版本
option(BUILD_WITH_GUI "Build with graphical user interface" OFF)
# 添加构建选项，控制是否构建测试程序
option(BUILD_WITH_TESTS "Build test programs" ON)

# 在项目定义后添加
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_UNICODE UNICODE)
    if(BUILD_WITH_GUI)
        add_compile_definitions(WXUSINGDLL)
        add_compile_definitions(__WXMSW__)
    endif()
endif()

# --- 构建测试程序 ---
if(BUILD_WITH_TESTS)
    # 创建模型读取器测试程序
    add_executable(ModelReaderTest
        model_reader_test.cpp
        src/model3d.cpp
        src/model_io.cpp
        src/mesh_processor.cpp
    )

    # 设置输出目录
    set_target_properties(ModelReaderTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # 添加包含目录
    target_include_directories(ModelReaderTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")


    # 添加测试数据复制命令
    add_custom_command(TARGET ModelReaderTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/test_models"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/Release/test_models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/test_models/cube.stl"
            "${CMAKE_BINARY_DIR}/bin/test_models/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/test_models/cube.stl"
            "${CMAKE_BINARY_DIR}/bin/Release/test_models/"
    )

    # 创建向量测试程序
    add_executable(VectorTest
        vector_test.cpp
    )

    # 设置输出目录
    set_target_properties(VectorTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # 添加包含目录
    target_include_directories(VectorTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    message(STATUS "Test programs will be built")
endif()