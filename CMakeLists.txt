cmake_minimum_required(VERSION 3.21)

project(SHHFix VERSION 0.1.0 LANGUAGES C CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "SHHFix se compile avec MSVC (Build Tools for Visual Studio).")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "SilentHill.exe est supposé 32 bits (à confirmer) : configurer en x86 (preset x86-*, ou -A Win32).")
endif()

# CRT statique : la DLL ne dépend d'aucun redistribuable Visual C++ installé.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# DLL du mod, sous le nom version.dll : SilentHill.exe importe VERSION.dll en premier (voir docs/analyse-shh-exe.md).
# de la table d'imports de SilentHill.exe : voir docs/a-faire.md. En attendant, elle se compile sous son
add_library(SHHFix SHARED
    src/dllmain.cpp
    src/core/Config.cpp
    src/core/Hooking.cpp
    src/core/Log.cpp
    src/core/Paths.cpp
    src/core/Strings.cpp
    src/core/SystemInfo.cpp
    src/crash/CrashHandler.cpp
    src/framepacing/FrameMonitor.cpp
    src/graphics/BorderlessWindow.cpp
    src/graphics/D3D9Hooks.cpp
    src/proxy/VersionProxy.cpp
    src/proxy/version.def
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

configure_file(src/version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

# Le jeu charge cette DLL à la place de la vraie version.dll (voir src/proxy/VersionProxy.cpp).
set_target_properties(SHHFix PROPERTIES OUTPUT_NAME "version")

target_include_directories(SHHFix PRIVATE src)
target_compile_features(SHHFix PRIVATE cxx_std_20)
target_compile_definitions(SHHFix PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    SHHF_VERSION="${PROJECT_VERSION}"
)
target_compile_options(SHHFix PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
target_link_options(SHHFix PRIVATE $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO /OPT:REF /OPT:ICF>)

# Paquet (build/<preset>/package) : copie faite par la cible qui produit la DLL, sinon elle n'est pas
# refaite quand seule la DLL change.
set(SHHF_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
add_custom_command(TARGET SHHFix POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHHF_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SHHFix> "${SHHF_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dist/SHHFix.ini" "${SHHF_PACKAGE_DIR}"
    # GPL-3.0 : le texte de la licence accompagne les binaires distribués.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "${SHHF_PACKAGE_DIR}/LICENSE.txt"
)

# Tests unitaires des fonctions pures, sans jeu ni matériel (tests/run-tests.ps1).
option(SHHF_BUILD_TESTS "Compiler les tests" ON)
if(SHHF_BUILD_TESTS)
    add_executable(SHHFixUnitTests tests/unit/main.cpp src/core/Strings.cpp src/graphics/BorderlessWindow.cpp src/core/Log.cpp)
    target_include_directories(SHHFixUnitTests PRIVATE src)
    target_compile_features(SHHFixUnitTests PRIVATE cxx_std_20)
    target_compile_definitions(SHHFixUnitTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(SHHFixUnitTests PRIVATE /W4 /WX /permissive- /utf-8)
endif()
