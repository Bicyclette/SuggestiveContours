CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

project(suggestive_contours)

SET(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Release FORCE)

add_executable(${PROJECT_NAME}
src/main.cpp
src/shader.cpp
src/mesh.cpp
src/camera.cpp
src/application.cpp
src/imgui/imgui.cpp
src/imgui/imgui_demo.cpp
src/imgui/imgui_draw.cpp
src/imgui/imgui_impl_glfw.cpp
src/imgui/imgui_impl_opengl3.cpp
src/imgui/imgui_tables.cpp
src/imgui/imgui_widgets.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dep/glad/src/glad.c)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dep/glad/include/)

add_subdirectory(dep/glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

add_subdirectory(dep/glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm)

include_directories(include)

target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})