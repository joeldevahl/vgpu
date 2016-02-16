set(common_HEADERS vgpu_internal.h ${PROJECT_SOURCE_DIR}/include/vgpu.h)
set(common_SOURCES vgpu.cpp)

set(vgpu_null_HEADERS ${common_HEADERS})
set(vgpu_null_SOURCES ${common_SOURCES} vgpu_null.cpp)

set(vgpu_gl_HEADERS ${common_HEADERS} vgpu_gl.h)
set(vgpu_gl_SOURCES ${common_SOURCES} vgpu_gl.cpp)

set(vgpu_vk_HEADERS ${common_HEADERS})
set(vgpu_vk_SOURCES ${common_SOURCES} vgpu_vk.cpp)

set(vgpu_dx11_HEADERS ${common_HEADERS})
set(vgpu_dx11_SOURCES ${common_SOURCES} vgpu_dx11.cpp)

set(vgpu_dx12_HEADERS ${common_HEADERS})
set(vgpu_dx12_SOURCES ${common_SOURCES} vgpu_dx12.cpp)

add_library(vgpu_null ${vgpu_null_SOURCES} ${vgpu_null_HEADERS})
target_include_directories(vgpu_null PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_library(vgpu_gl ${vgpu_gl_SOURCES} ${vgpu_gl_HEADERS})
target_include_directories(vgpu_gl PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_library(vgpu_vk ${vgpu_vk_SOURCES} ${vgpu_vk_HEADERS})
target_include_directories(vgpu_vk PRIVATE ${PROJECT_SOURCE_DIR}/include C:/VulkanSDK/1.0.3.1/Include)

add_library(vgpu_dx11 ${vgpu_dx11_SOURCES} ${vgpu_dx11_HEADERS})
target_include_directories(vgpu_dx11 PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_library(vgpu_dx12 ${vgpu_dx12_SOURCES} ${vgpu_dx12_HEADERS})
target_include_directories(vgpu_dx12 PRIVATE ${PROJECT_SOURCE_DIR}/include)