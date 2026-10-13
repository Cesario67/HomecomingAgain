cmake_minimum_required(VERSION 3.21)

# Programme DLSS 64 bits (HomecomingAgain.Dlss.exe) : le DLSS n'existe qu'en 64 bits, le jeu est 32 bits (voir
# docs/plan-dlss.md). Compilé à part, en x64, par tools/build.ps1.
project(HomecomingAgainDlss LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Le programme DLSS se compile en 64 bits (vcvarsall amd64).")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# En-têtes Vulkan (Khronos, Apache-2.0), même version et même empreinte que pour la DLL.
include(FetchContent)
FetchContent_Declare(vulkan_headers
    URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
    URL_HASH SHA256=4A078BE12BEF21CFEBC09D878B77A63CFF9D68F899254A0B00D0E37EF73E7F7E)
FetchContent_MakeAvailable(vulkan_headers)

add_executable(HomecomingAgainDlss WIN32 src/main.cpp src/Arguments.cpp src/SharedFrameReader.cpp src/BmpWriter.cpp src/MotionCheck.cpp)
set_target_properties(HomecomingAgainDlss PROPERTIES OUTPUT_NAME "HomecomingAgain.Dlss")
target_include_directories(HomecomingAgainDlss PRIVATE src "${vulkan_headers_SOURCE_DIR}/include")
target_compile_features(HomecomingAgainDlss PRIVATE cxx_std_20)
target_compile_definitions(HomecomingAgainDlss PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(HomecomingAgainDlss PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)

add_executable(HomecomingAgainDlssTests tests/main.cpp src/Arguments.cpp src/BmpWriter.cpp src/MotionCheck.cpp)
target_include_directories(HomecomingAgainDlssTests PRIVATE src)
target_compile_features(HomecomingAgainDlssTests PRIVATE cxx_std_20)
target_compile_definitions(HomecomingAgainDlssTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(HomecomingAgainDlssTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
