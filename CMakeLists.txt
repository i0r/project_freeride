cmake_minimum_required( VERSION 3.5 FATAL_ERROR )
cmake_policy( VERSION 3.5 )

project( Dusk )

set_property (GLOBAL PROPERTY USE_FOLDERS ON)

set( DUSK_BASE_FOLDER "${CMAKE_SOURCE_DIR}/" )

set( DUSK_BIN_FOLDER "${DUSK_BASE_FOLDER}/build/bin" )
set( DUSK_LIB_FOLDER "${DUSK_BASE_FOLDER}/build/lib" )

# Set global debug prefix
# Note that for some reason, CMake applies this prefix to libraries only
set( CMAKE_DEBUG_POSTFIX "_Debug" )
set( CMAKE_CXX_STANDARD 14 )

# Set output paths
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${DUSK_LIB_FOLDER} )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${DUSK_LIB_FOLDER} )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${DUSK_LIB_FOLDER} )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${DUSK_LIB_FOLDER} )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${DUSK_LIB_FOLDER} )

set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${DUSK_LIB_FOLDER} )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${DUSK_LIB_FOLDER} )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${DUSK_LIB_FOLDER} )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${DUSK_LIB_FOLDER} )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${DUSK_LIB_FOLDER} )

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${DUSK_BIN_FOLDER} )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${DUSK_BIN_FOLDER} )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${DUSK_BIN_FOLDER} )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${DUSK_BIN_FOLDER} )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${DUSK_BIN_FOLDER} )

# Include CMake Modules
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${DUSK_BASE_FOLDER}/CMakeModules" )

include( ${CMAKE_MODULE_PATH}/PrecompiledHeader.cmake )
include( ${CMAKE_MODULE_PATH}/MSVCFilters.cmake )
include( ${CMAKE_MODULE_PATH}/UnityBuild.cmake )
include( ${CMAKE_MODULE_PATH}/FindFBXSDK.cmake )

# Build Parameters
set( DUSK_GFX_API "DUSK_D3D12" CACHE STRING "Graphics Backend" )
set( DUSK_DEVBUILD TRUE CACHE BOOL "Build with developper features" )
set( DUSK_USE_FBXSDK FALSE CACHE BOOL "Use FBX SDK for geometry baking" )
set( DUSK_USE_STB_IMAGE TRUE CACHE BOOL "Use stb_image headers for bitmap parsing/writing." )
set( DUSK_USE_BULLET TRUE CACHE BOOL "Use Bullet Physics as physics library" )
set( DUSK_USE_NVAPI TRUE CACHE BOOL "Build with Nvidia API (NVAPI)" )
set( DUSK_USE_AGS TRUE CACHE BOOL "Build with AMD GPU Services (AGS)" )
set( DUSK_USE_UNITY_BUILD TRUE CACHE BOOL "Use UnityBuild for project compilation" )
set( DUSK_ENABLE_GPU_DEBUG_MARKER TRUE CACHE BOOL "Enable GPU Debug Markers (resource/region markers, etc.)" )
set( DUSK_USE_DIRECTX_COMPILER TRUE CACHE BOOL "Use DirectXCompiler for shader compilation (SPIRV/DXC bytecode)" )

set_property(CACHE DUSK_GFX_API PROPERTY STRINGS DUSK_D3D11 DUSK_D3D12 DUSK_VULKAN DUSK_STUB)

# System specifics
if ( WIN32 )
    add_definitions( -DDUSK_WIN ) 
    add_definitions( -DDUSK_WINAPI )

    add_definitions( -D_CRT_SECURE_NO_WARNINGS ) 
    add_definitions( -D_SCL_SECURE_NO_WARNINGS ) 
    add_definitions( -DUNICODE -D_UNICODE ) 
    add_definitions( -DDUSK_UNICODE )
elseif( UNIX )
    add_definitions( -DDUSK_UNIX )
    add_definitions( -DDUSK_XCB )
endif( WIN32 )

if ( MSVC )
    add_definitions( -DDUSK_MSVC )
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions( -DDUSK_CLANG )
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions( -DDUSK_GCC )
endif ( MSVC )

if ( DUSK_DEVBUILD )
    add_definitions( -DDUSK_DEVBUILD )
endif ( DUSK_DEVBUILD )

if ( DUSK_USE_FBXSDK )
    add_definitions( -DDUSK_USE_FBXSDK )
endif ( DUSK_USE_FBXSDK )

if ( DUSK_ENABLE_GPU_DEBUG_MARKER )
    add_definitions( -DDUSK_ENABLE_GPU_DEBUG_MARKER )
endif ( DUSK_ENABLE_GPU_DEBUG_MARKER )

if ( DUSK_USE_BULLET )
    add_definitions( -DDUSK_USE_BULLET )
    
    if ( UNIX )
        find_package(Bullet REQUIRED)
        include_directories(${BULLET_INCLUDE_DIR})
    endif ( UNIX )
endif ( DUSK_USE_BULLET )

if ( DUSK_USE_NVAPI )
    add_definitions( -DDUSK_USE_NVAPI )
endif ( DUSK_USE_NVAPI )

if ( DUSK_USE_AGS )
    add_definitions( -DDUSK_USE_AGS )
endif ( DUSK_USE_AGS )

if ( DUSK_USE_DIRECTX_COMPILER )
    add_definitions( -DDUSK_USE_DIRECTX_COMPILER )
endif ( DUSK_USE_DIRECTX_COMPILER )

if ( DUSK_USE_STB_IMAGE )
    add_definitions( -DDUSK_USE_STB_IMAGE )
endif( DUSK_USE_STB_IMAGE )

add_definitions( -D${DUSK_GFX_API} )

# Add shared include directories (add paths here only if needed)
include_directories( "${DUSK_BASE_FOLDER}/" )
link_directories( "${DUSK_BASE_FOLDER}/build/lib" )

# Add stuff to build below
add_subdirectory( Dusk )
add_subdirectory( DuskEd )
add_subdirectory( DuskBaker )
add_subdirectory( DuskTestGraphics )
