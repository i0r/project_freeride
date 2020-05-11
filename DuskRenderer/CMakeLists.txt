file(GLOB_RECURSE SRC 
    "${DUSK_BASE_FOLDER}DuskRenderer/Graphics/*.cpp"
    "${DUSK_BASE_FOLDER}DuskRenderer/Rendering/*.cpp"
    "${DUSK_BASE_FOLDER}DuskRenderer/ThirdParty/*.cpp"
    "${DUSK_BASE_FOLDER}DuskRenderer/ThirdParty/*.c" )
file(GLOB_RECURSE INC 
    "${DUSK_BASE_FOLDER}DuskRenderer/Graphics/*.h"
    "${DUSK_BASE_FOLDER}DuskRenderer/Rendering/*.h"
    "${DUSK_BASE_FOLDER}DuskRenderer/ThirdParty/*.h"
    "${DUSK_BASE_FOLDER}DuskRenderer/ThirdParty/*.hpp"
    "${DUSK_BASE_FOLDER}DuskRenderer/ThirdParty/*.inl")

set( SOURCES ${SRC} ${INC} )

if ( ${DUSK_USE_UNITY_BUILD} )
	enable_unity_build( DuskRenderer SRC 16 cpp)
endif ( ${DUSK_USE_UNITY_BUILD} )

set( SOURCES ${SRC} ${INC} )

add_library( DuskRenderer STATIC ${SOURCES} )

include_directories( "${DUSK_BASE_FOLDER}DuskRenderer" )
include_directories( "${DUSK_BASE_FOLDER}DuskCore" )
include_directories( "${DUSK_BASE_FOLDER}DuskRenderer/ThirdParty/" )

if ( "${DUSK_GFX_API}" MATCHES "DUSK_D3D12" )
    find_package( DirectX REQUIRED )

    target_link_libraries( DuskRenderer d3d12 )
    target_link_libraries( DuskRenderer dxgi )
	
    target_link_libraries( DuskRenderer d3dcompiler )
        
    set_target_properties( DuskRenderer PROPERTIES DEBUG_OUTPUT_NAME "DuskRenderer" )
    set_target_properties( DuskRenderer PROPERTIES RELEASE_OUTPUT_NAME "DuskRenderer" )
    set_target_properties( DuskRenderer PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "DuskRenderer" )

    add_definitions( -DDUSK_ASYNC_COMPUTE_AVAILABLE )

if ( DUSK_DEVBUILD )
    target_link_libraries( DuskRenderer dxguid )
endif( DUSK_DEVBUILD )
elseif ( "${DUSK_GFX_API}" MATCHES "DUSK_VULKAN" )
    find_package( Vulkan REQUIRED )
    include_directories( ${Vulkan_INCLUDE_DIR} )
        
    set_target_properties( DuskRenderer PROPERTIES DEBUG_OUTPUT_NAME "DuskRenderer" )
    set_target_properties( DuskRenderer PROPERTIES RELEASE_OUTPUT_NAME "DuskRenderer" )
    set_target_properties( DuskRenderer PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "DuskRenderer" )

    add_definitions( -DDUSK_ASYNC_COMPUTE_AVAILABLE )

    target_link_libraries( DuskRenderer Vulkan::Vulkan )
elseif ( "${DUSK_GFX_API}" MATCHES "DUSK_D3D11" )
    find_package( DirectX REQUIRED )

    target_link_libraries( DuskRenderer d3d11 )
    target_link_libraries( DuskRenderer dxgi )
	
    target_link_libraries( DuskRenderer d3dcompiler )
        
    set_target_properties( DuskRenderer PROPERTIES DEBUG_OUTPUT_NAME "DuskRenderer" )
    set_target_properties( DuskRenderer PROPERTIES RELEASE_OUTPUT_NAME "DuskRenderer" )
    set_target_properties( DuskRenderer PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "DuskRenderer" )

if ( DUSK_DEVBUILD )
    target_link_libraries( DuskRenderer dxguid )
endif( DUSK_DEVBUILD )
endif( "${DUSK_GFX_API}" MATCHES "DUSK_D3D12" )

if(MSVC)
  target_compile_options(DuskRenderer PRIVATE /W3 /WX)
else()
  target_compile_options(DuskRenderer PRIVATE -Wall -Wextra)
endif()

add_msvc_filters( "${SOURCES}" )

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET DuskRenderer
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}DuskRenderer/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
