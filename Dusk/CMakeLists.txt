file(GLOB_RECURSE SRC "${DUSK_BASE_FOLDER}Dusk/Core/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/FileSystem/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Framework/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Input/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Io/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Maths/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Parsing/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Graphics/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Rendering/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Physics/*.cpp")
file(GLOB_RECURSE INC "${DUSK_BASE_FOLDER}Dusk/Core/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Core/*.inl"
    "${DUSK_BASE_FOLDER}Dusk/FileSystem/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Framework/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Input/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Io/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Maths/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Maths/*.inl"
    "${DUSK_BASE_FOLDER}Dusk/Parsing/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Rendering/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Graphics/*.h"
    "${DUSK_BASE_FOLDER}Dusk/Physics/*.h")
    
if ( DUSK_USE_BULLET )
file(GLOB_RECURSE BULLET_HEADERS 
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/*.hpp"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/*.h" )
endif ( DUSK_USE_BULLET )

if ( DUSK_USE_NVAPI )
    file(GLOB_RECURSE NVAPI_HEADERS 
        "${DUSK_BASE_FOLDER}Dusk/ThirdParty/nvapi/*.h" )
endif ( DUSK_USE_NVAPI )

if ( DUSK_USE_AGS )
    file(GLOB AGS_HEADERS "${DUSK_BASE_FOLDER}Dusk/ThirdParty/ags/amd_ags.h" )
endif ( DUSK_USE_AGS )

file(GLOB_RECURSE SRC_THIRD_PARTY 
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/SPIRV-Cross/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/SPIRV-Cross/*.c"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/SPIRV-Cross/*.hpp"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/SPIRV-Cross/*.h"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/dxc/*.hpp"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/dxc/*.h"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/stb_image.h"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/stb_image_write.h"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/miniz/*.c"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/miniz/*.h" )
    
set( SOURCES ${SRC} ${INC} "${DUSK_BASE_FOLDER}Dusk/Shared.cpp" "${DUSK_BASE_FOLDER}Dusk/Shared.h" )
add_precompiled_header( "${SOURCES}" Shared.h )

if ( ${DUSK_USE_UNITY_BUILD} )
	enable_unity_build( Dusk SRC 16 cpp)
endif ( ${DUSK_USE_UNITY_BUILD} )

set( SOURCES ${SRC} ${INC} ${AGS_HEADERS} ${BULLET_HEADERS} ${SRC_THIRD_PARTY} ${NVAPI_HEADERS} "${DUSK_BASE_FOLDER}Dusk/Shared.cpp" "${DUSK_BASE_FOLDER}Dusk/Shared.h" )

add_library( Dusk STATIC ${SOURCES} )

set_property(TARGET Dusk PROPERTY FOLDER "DuskEngine")

include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty" )
include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/miniz" )
include_directories( "${DUSK_BASE_FOLDER}Dusk" )

if(MSVC)
  target_compile_options(Dusk PRIVATE /W3 /WX)
else()
  target_compile_options(Dusk PRIVATE -Wall -Wextra)
endif()

add_msvc_filters( "${SOURCES}" )

if ( "${DUSK_GFX_API}" MATCHES "DUSK_D3D12" )
    find_package( DirectX REQUIRED )

    target_link_libraries( Dusk d3d12 )
    target_link_libraries( Dusk dxgi )
	
    target_link_libraries( Dusk d3dcompiler )
        
    set_target_properties( Dusk PROPERTIES DEBUG_OUTPUT_NAME "Dusk" )
    set_target_properties( Dusk PROPERTIES RELEASE_OUTPUT_NAME "Dusk" )
    set_target_properties( Dusk PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "Dusk" )

    add_definitions( -DDUSK_ASYNC_COMPUTE_AVAILABLE )

if ( DUSK_DEVBUILD )
    target_link_libraries( Dusk dxguid )
endif( DUSK_DEVBUILD )
elseif ( "${DUSK_GFX_API}" MATCHES "DUSK_VULKAN" )
    find_package( Vulkan REQUIRED )
    include_directories( ${Vulkan_INCLUDE_DIR} )
        
    set_target_properties( Dusk PROPERTIES DEBUG_OUTPUT_NAME "Dusk" )
    set_target_properties( Dusk PROPERTIES RELEASE_OUTPUT_NAME "Dusk" )
    set_target_properties( Dusk PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "Dusk" )

    add_definitions( -DDUSK_ASYNC_COMPUTE_AVAILABLE )

    target_link_libraries( Dusk Vulkan::Vulkan )
elseif ( "${DUSK_GFX_API}" MATCHES "DUSK_D3D11" )
    find_package( DirectX REQUIRED )

    target_link_libraries( Dusk d3d11 )
    target_link_libraries( Dusk dxgi )
	
    target_link_libraries( Dusk d3dcompiler )
        
    set_target_properties( Dusk PROPERTIES DEBUG_OUTPUT_NAME "Dusk" )
    set_target_properties( Dusk PROPERTIES RELEASE_OUTPUT_NAME "Dusk" )
    set_target_properties( Dusk PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "Dusk" )

if ( DUSK_DEVBUILD )
    target_link_libraries( Dusk dxguid )
endif( DUSK_DEVBUILD )
endif( "${DUSK_GFX_API}" MATCHES "DUSK_D3D12" )

if ( DUSK_USE_BULLET )
include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/src" )

target_link_libraries( Dusk 
                        debug "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/Debug/BulletDynamics_Debug.lib" 
                        optimized "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/RelWithDebInfo/BulletDynamics_RelWithDebugInfo.lib" ) 
                        
target_link_libraries( Dusk 
                        debug "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/Debug/BulletCollision_Debug.lib" 
                        optimized "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/RelWithDebInfo/BulletCollision_RelWithDebugInfo.lib" ) 
                        
target_link_libraries( Dusk 
                        debug "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/Debug/LinearMath_Debug.lib" 
                        optimized "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/RelWithDebInfo/LinearMath_RelWithDebugInfo.lib" ) 
endif ( DUSK_USE_BULLET )

if ( DUSK_USE_NVAPI )
    include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/nvapi" )
    target_link_libraries( Dusk "${DUSK_BASE_FOLDER}Dusk/ThirdParty/nvapi/amd64/nvapi64.lib" ) 
endif ( DUSK_USE_NVAPI )

if ( DUSK_USE_AGS )
target_link_libraries( Dusk 
                        debug "${DUSK_BASE_FOLDER}Dusk/ThirdParty/ags/lib/amd_ags_x64_2019_MDd.lib" 
                        optimized "${DUSK_BASE_FOLDER}Dusk/ThirdParty/ags/lib/amd_ags_x64_2019_MD.lib" ) 
endif ( DUSK_USE_AGS )

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET Dusk
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}Dusk/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
