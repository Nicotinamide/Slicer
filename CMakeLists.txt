# E:/CodesE/Slicer/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(MySlicerProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 在项目定义后添加
if(MSVC)
    add_compile_options(/utf-8)
endif()

# --- 查找 Boost ---
find_package(Boost REQUIRED
    COMPONENTS system filesystem thread date_time regex # 根据你的需求
)

# --- 查找 CGAL ---
find_package(CGAL REQUIRED)

# --- 查找 wxWidgets ---
find_package(wxWidgets REQUIRED
    COMPONENTS core base gl # 添加 gl 组件
    # OPTIONAL_COMPONENTS net html # 其他可选组件
)

# --- 查找 Assimp ---
# vcpkg 通常会提供一个 CMake 配置文件 (assimp-config.cmake 或 assimpConfig.cmake)，
# 所以使用 CONFIG 模式通常是最佳选择。
find_package(assimp CONFIG REQUIRED) # Assimp 官方推荐小写包名

# 添加 OpenGL 支持
find_package(OpenGL REQUIRED)

# 查找 GLEW
find_package(GLEW REQUIRED)

# --- 查找 GLM 数学库 ---
find_package(glm CONFIG REQUIRED)
message(STATUS "GLM found")


# --- 添加可执行文件 ---
add_executable(MySlicer
    # main.cpp # 如果你决定不再使用根目录的main.cpp作为入口
    src/gui_main.cpp          # 包含 wxWidgets 的 MyApp 和 MyFrame 定义
    src/slicer_core.cpp       # 核心非GUI逻辑
    src/model_loader.cpp      # 包含 Assimp 加载逻辑
    src/model_viewer.cpp      # 添加新文件
)

set_target_properties(MySlicer PROPERTIES
    WIN32_EXECUTABLE FALSE)  # FALSE = 显示控制台窗口

# --- 链接库 ---
if(Boost_FOUND)
    message(STATUS "Boost found. Version: ${Boost_VERSION_STRING}")
    target_link_libraries(MySlicer PRIVATE
        Boost::system
        Boost::filesystem
        Boost::thread
        Boost::date_time
        Boost::regex
    )
endif()

if(CGAL_FOUND)
    message(STATUS "CGAL found. Version: ${CGAL_VERSION_STRING}")
    target_link_libraries(MySlicer PRIVATE CGAL::CGAL)
endif()

if(wxWidgets_FOUND)
    message(STATUS "wxWidgets found. Version: ${wxWidgets_VERSION_STRING}")
    message(STATUS "wxWidgets_INCLUDE_DIRS: ${wxWidgets_INCLUDE_DIRS}")
    message(STATUS "wxWidgets_LIBRARIES: ${wxWidgets_LIBRARIES}") # 留意这条命令在CMake输出中的实际内容
    message(STATUS "wxWidgets_DEFINITIONS: ${wxWidgets_DEFINITIONS}")

    target_include_directories(MySlicer PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(MySlicer PRIVATE ${wxWidgets_DEFINITIONS})
    target_link_libraries(MySlicer PRIVATE ${wxWidgets_LIBRARIES})
else()
    message(FATAL_ERROR "wxWidgets was not found.")
endif()

if(assimp_FOUND) # 注意 find_package(assimp ...) 会设置 assimp_FOUND (小写)
    message(STATUS "Assimp found. Version: ${assimp_VERSION}") # 通常版本变量是 <PackageName>_VERSION
    # Assimp 通常提供一个导入目标 assimp::assimp
    target_link_libraries(MySlicer PRIVATE assimp::assimp)
endif()

# 在链接库部分添加 OpenGL
target_link_libraries(MySlicer PRIVATE OpenGL::GL)

# 链接 GLEW
target_link_libraries(MySlicer PRIVATE GLEW::GLEW)

# 链接 glm:
target_link_libraries(MySlicer PRIVATE glm::glm)

# 确保你的源文件能找到 src/ 目录下的头文件
target_include_directories(MySlicer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")