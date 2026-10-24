cmake_minimum_required(VERSION 3.21)

project(HomecomingAgain VERSION 0.9.8 LANGUAGES C CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "HomecomingAgain se compile avec MSVC (Build Tools for Visual Studio).")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "SilentHill.exe est supposé 32 bits (à confirmer) : configurer en x86 (preset x86-*, ou -A Win32).")
endif()

# CRT statique : la DLL ne dépend d'aucun redistribuable Visual C++ installé.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


# --- En-têtes Vulkan (Khronos, Apache-2.0) : déclarations seulement, pour l'interopérabilité DXVK (dlss/) ------------
# Téléchargés par FetchContent, version figée et empreinte SHA-256 vérifiée ; jamais stockés dans le dépôt.
include(FetchContent)
FetchContent_Declare(vulkan_headers
    URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
    URL_HASH SHA256=4A078BE12BEF21CFEBC09D878B77A63CFF9D68F899254A0B00D0E37EF73E7F7E)
FetchContent_MakeAvailable(vulkan_headers)
set(HCA_VULKAN_INCLUDE "${vulkan_headers_SOURCE_DIR}/include")
# --- Shaders SMAA (Jimenez et al., BSD, third_party/smaa) compilés par fxc ----------------------------------------
# Aucune dépendance à D3DX à l'exécution : chaque point d'entrée devient un en-tête « const BYTE g_<nom>[] ».
find_program(HCA_FXC fxc HINTS "$ENV{WindowsSdkVerBinPath}x86" "$ENV{WindowsSdkVerBinPath}x64" REQUIRED)
set(HCA_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/smaa_shaders")
set(HCA_SHADER_HEADERS)
foreach(entry EdgeDetectionVS:vs_3_0 BlendingWeightCalculationVS:vs_3_0 NeighborhoodBlendingVS:vs_3_0
              LumaEdgeDetectionPS:ps_3_0 BlendingWeightCalculationPS:ps_3_0 NeighborhoodBlendingPS:ps_3_0)
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 profile)
    set(header "${HCA_SHADER_DIR}/smaa_shaders/${name}.h")
    add_custom_command(OUTPUT "${header}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${HCA_SHADER_DIR}/smaa_shaders"
        COMMAND "${HCA_FXC}" /nologo /O3 /T ${profile} /E ${name} /I "${CMAKE_SOURCE_DIR}/third_party/smaa"
                /Vn g_${name} /Fh "${header}" "${CMAKE_SOURCE_DIR}/shaders/smaa_d3d9.hlsl"
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/smaa_d3d9.hlsl" "${CMAKE_SOURCE_DIR}/third_party/smaa/SMAA.h"
        COMMENT "fxc ${name} (${profile})"
        VERBATIM)
    list(APPEND HCA_SHADER_HEADERS "${header}")
endforeach()
# SSAO (shaders/ssao_d3d9.hlsl, notre code), même méthode.
foreach(entry QuadVS:vs_3_0 OcclusionPS:ps_3_0 BlurPS:ps_3_0)
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 profile)
    set(header "${HCA_SHADER_DIR}/ssao_shaders/${name}.h")
    add_custom_command(OUTPUT "${header}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${HCA_SHADER_DIR}/ssao_shaders"
        COMMAND "${HCA_FXC}" /nologo /O3 /T ${profile} /E ${name} /Vn g_${name} /Fh "${header}"
                "${CMAKE_SOURCE_DIR}/shaders/ssao_d3d9.hlsl"
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/ssao_d3d9.hlsl"
        COMMENT "fxc ${name} (${profile})"
        VERBATIM)
    list(APPEND HCA_SHADER_HEADERS "${header}")
endforeach()
# Vecteurs de mouvement pour le DLSS (shaders/motion_d3d9.hlsl, notre code), même méthode.
foreach(entry MotionQuadVS:vs_3_0 MotionPS:ps_3_0)
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 profile)
    set(header "${HCA_SHADER_DIR}/motion_shaders/${name}.h")
    add_custom_command(OUTPUT "${header}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${HCA_SHADER_DIR}/motion_shaders"
        COMMAND "${HCA_FXC}" /nologo /O3 /T ${profile} /E ${name} /Vn g_${name} /Fh "${header}"
                "${CMAKE_SOURCE_DIR}/shaders/motion_d3d9.hlsl"
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/motion_d3d9.hlsl"
        COMMENT "fxc ${name} (${profile})"
        VERBATIM)
    list(APPEND HCA_SHADER_HEADERS "${header}")
endforeach()
add_custom_target(smaa_shaders DEPENDS ${HCA_SHADER_HEADERS})
# DLL du mod, sous le nom version.dll : SilentHill.exe importe VERSION.dll en premier (voir docs/analyse-shh-exe.md).
# de la table d'imports de SilentHill.exe : voir docs/a-faire.md. En attendant, elle se compile sous son
add_library(HomecomingAgain SHARED
    src/dllmain.cpp
    src/audio/AudioProbe.cpp
    src/input/XInputHooks.cpp
    src/input/MenuBridge.cpp
    src/input/SonyPad.cpp
    src/input/DirectInputHooks.cpp
    src/input/ButtonIcons.cpp
    src/input/ControllerNames.cpp
    src/input/EngineUiState.cpp
    src/input/InputLog.cpp
   
    src/compat/MovieSkip.cpp
    src/compat/StartupLogos.cpp
    src/compat/SkipMovies.cpp
    src/compat/WindowThreadLock.cpp
    src/core/Config.cpp
    src/core/Hooking.cpp
    src/core/Log.cpp
    src/core/Paths.cpp
    src/core/Strings.cpp
    src/core/SystemInfo.cpp
    src/crash/CrashHandler.cpp
    src/crash/HangWatchdog.cpp
    src/crash/HangClock.cpp
    src/dlss/DxvkInterop.cpp
    src/dlss/SharedFrames.cpp
    src/engine/EngineSettings.cpp
    src/framepacing/FrameMonitor.cpp
    src/framepacing/WaitProfiler.cpp
    src/graphics/BackBufferDump.cpp
    src/graphics/BorderlessWindow.cpp
    src/graphics/CameraMath.cpp
    src/graphics/CameraTracker.cpp
    src/graphics/DepthEncoding.cpp
    src/graphics/Jitter.cpp
    src/graphics/D3D9Hooks.cpp
    src/graphics/D3D9On12.cpp
    src/graphics/FilmGrain.cpp
    src/graphics/FrameCapture.cpp
    src/graphics/MotionVectors.cpp
    src/graphics/Projection.cpp
    src/graphics/SceneHooks.cpp
    src/graphics/Dxt3.cpp
    src/graphics/FlashIconArt.cpp
    src/graphics/FlashIcons.cpp
    src/graphics/ShadowResolution.cpp
    src/graphics/Ssao.cpp
    src/graphics/UiClip.cpp
    src/graphics/ShaderConstantTable.cpp
    src/graphics/Smaa.cpp
    src/patches/CameraFix.cpp
    src/patches/CameraTuning.cpp
    src/patches/Cheats.cpp
    src/patches/FrameRateCap.cpp
    src/proxy/VersionProxy.cpp
    src/proxy/version.def
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

configure_file(src/version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

# Le jeu charge cette DLL à la place de la vraie version.dll (voir src/proxy/VersionProxy.cpp).
set_target_properties(HomecomingAgain PROPERTIES OUTPUT_NAME "version")

target_include_directories(HomecomingAgain PRIVATE src third_party/smaa third_party/dxvk "${HCA_SHADER_DIR}" "${HCA_VULKAN_INCLUDE}")
add_dependencies(HomecomingAgain smaa_shaders)
target_compile_features(HomecomingAgain PRIVATE cxx_std_20)
target_compile_definitions(HomecomingAgain PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    HCA_VERSION="${PROJECT_VERSION}"
)
target_compile_options(HomecomingAgain PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
target_link_options(HomecomingAgain PRIVATE $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO /OPT:REF /OPT:ICF>)

# Paquet (build/<preset>/package) : copie faite par la cible qui produit la DLL, sinon elle n'est pas
# refaite quand seule la DLL change.
set(HCA_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
add_custom_command(TARGET HomecomingAgain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${HCA_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:HomecomingAgain> "${HCA_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dist/HomecomingAgain.ini" "${HCA_PACKAGE_DIR}"
    # GPL-3.0 : le texte de la licence accompagne les binaires distribués.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "${HCA_PACKAGE_DIR}/LICENSE.txt"
    # Licence de SMAA : mention obligatoire pour la distribution binaire.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/third_party/smaa/LICENSE.txt" "${HCA_PACKAGE_DIR}/LICENSE-SMAA.txt"
    # Licence MIT du Camera Fix (patches/CameraFix.cpp) : son avis de copyright accompagne les binaires.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/third_party/camerafix/LICENSE.txt" "${HCA_PACKAGE_DIR}/LICENSE-CAMERAFIX.txt"
)

# DXVK 3.1.1 (Direct3D 9 sur Vulkan, licence zlib) : nécessaire au DLSS, installé par le launcher seulement quand
# l'option DLSS est activée. Archive officielle, empreinte vérifiée (identique à celle publiée par GitHub).
FetchContent_Declare(dxvk_release
    URL https://github.com/doitsujin/dxvk/releases/download/v3.1.1/dxvk-3.1.1.tar.gz
    URL_HASH SHA256=40565B4A724AADC4433FA4E010B4B23916D9B1F1BAEEE64E17186DB94F54E608)
FetchContent_MakeAvailable(dxvk_release)
add_custom_command(TARGET HomecomingAgain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dxvk_release_SOURCE_DIR}/x32/d3d9.dll" "${HCA_PACKAGE_DIR}/d3d9.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/third_party/dxvk/LICENSE.txt" "${HCA_PACKAGE_DIR}/LICENSE-DXVK.txt"
)

# Tests unitaires des fonctions pures, sans jeu ni matériel (tests/run-tests.ps1).
option(HCA_BUILD_TESTS "Compiler les tests" ON)
if(HCA_BUILD_TESTS)
    add_executable(HomecomingAgainUnitTests tests/unit/main.cpp src/patches/CameraTuning.cpp src/core/Strings.cpp src/graphics/BorderlessWindow.cpp src/core/Log.cpp src/patches/FrameRateCap.cpp src/core/Config.cpp src/core/Hooking.cpp src/graphics/ShaderConstantTable.cpp src/graphics/ShadowResolution.cpp src/graphics/Projection.cpp src/graphics/FilmGrain.cpp src/compat/SkipMovies.cpp src/compat/MovieSkip.cpp src/compat/StartupLogos.cpp src/audio/AudioProbe.cpp src/graphics/CameraMath.cpp src/graphics/CameraTracker.cpp src/graphics/DepthEncoding.cpp src/graphics/Jitter.cpp src/input/MenuBridge.cpp src/input/SonyPad.cpp src/input/ButtonIcons.cpp src/input/ControllerNames.cpp src/input/EngineUiState.cpp src/input/InputLog.cpp src/core/Paths.cpp src/crash/HangClock.cpp src/graphics/UiClip.cpp src/graphics/Dxt3.cpp src/graphics/FlashIconArt.cpp src/graphics/FlashIcons.cpp)
    target_include_directories(HomecomingAgainUnitTests PRIVATE src)
    target_compile_features(HomecomingAgainUnitTests PRIVATE cxx_std_20)
    target_compile_definitions(HomecomingAgainUnitTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(HomecomingAgainUnitTests PRIVATE /W4 /WX /permissive- /utf-8)
endif()
