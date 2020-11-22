Macro (Dusk_UsePhysics TargetName)
    add_definitions( -DDUSK_USE_BULLET )

    if ( WIN32 )
        include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/include" )
        
        target_link_libraries( ${TargetName}
                                debug "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/lib/BulletDynamics_Debug.lib"
                                optimized "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/lib/BulletDynamics_RelWithDebugInfo.lib" )

        target_link_libraries( ${TargetName}
                                debug "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/lib/BulletCollision_Debug.lib"
                                optimized "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/lib/BulletCollision_RelWithDebugInfo.lib" )

        target_link_libraries( ${TargetName}
                                debug "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/lib/LinearMath_Debug.lib"
                                optimized "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/lib/LinearMath_RelWithDebugInfo.lib" )
    else()
        find_package(Bullet REQUIRED)
        include_directories(${BULLET_INCLUDE_DIR})
        target_link_libraries( ${TargetName} ${BULLET_LIBRARIES} )
    endif( WIN32 )
endMacro()

Macro (Dusk_UseRendering TargetName)
if ( DUSK_USE_NVAPI )
    add_definitions( -DDUSK_USE_NVAPI )
endif ( DUSK_USE_NVAPI )

if ( DUSK_USE_AGS )
    add_definitions( -DDUSK_USE_AGS )
endif ( DUSK_USE_AGS )

add_definitions( -D${DUSK_GFX_API} )

if ( "${DUSK_GFX_API}" MATCHES "DUSK_D3D12" )
    find_package( DirectX REQUIRED )

    target_link_libraries( ${TargetName} d3d12 )
    target_link_libraries( ${TargetName} dxgi )
	
    set_target_properties( ${TargetName} PROPERTIES DEBUG_OUTPUT_NAME "${TargetName}" )
    set_target_properties( ${TargetName} PROPERTIES RELEASE_OUTPUT_NAME "${TargetName}" )
    set_target_properties( ${TargetName} PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "${TargetName}" )

    add_definitions( -DDUSK_ASYNC_COMPUTE_AVAILABLE )

if ( DUSK_DEVBUILD )
    target_link_libraries( ${TargetName} dxguid )
endif( DUSK_DEVBUILD )
elseif ( "${DUSK_GFX_API}" MATCHES "DUSK_VULKAN" )
    find_package( Vulkan REQUIRED )
    include_directories( ${Vulkan_INCLUDE_DIR} )
        
    set_target_properties( ${TargetName} PROPERTIES DEBUG_OUTPUT_NAME "${TargetName}" )
    set_target_properties( ${TargetName} PROPERTIES RELEASE_OUTPUT_NAME "${TargetName}" )
    set_target_properties( ${TargetName} PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "${TargetName}" )

    add_definitions( -DDUSK_ASYNC_COMPUTE_AVAILABLE )

    target_link_libraries( ${TargetName} Vulkan::Vulkan )
elseif ( "${DUSK_GFX_API}" MATCHES "DUSK_D3D11" )
    find_package( DirectX REQUIRED )

    target_link_libraries( ${TargetName} d3d11 )
    target_link_libraries( ${TargetName} dxgi )
	
    set_target_properties( ${TargetName} PROPERTIES DEBUG_OUTPUT_NAME "${TargetName}" )
    set_target_properties( ${TargetName} PROPERTIES RELEASE_OUTPUT_NAME "${TargetName}" )
    set_target_properties( ${TargetName} PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "${TargetName}" )

if ( DUSK_DEVBUILD )
    target_link_libraries( ${TargetName} dxguid )
endif( DUSK_DEVBUILD )
endif( "${DUSK_GFX_API}" MATCHES "DUSK_D3D12" )

if ( DUSK_USE_NVAPI )
    include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/nvapi/include" )
    target_link_libraries( ${TargetName} "${DUSK_BASE_FOLDER}Dusk/ThirdParty/nvapi/lib/nvapi64.lib" ) 
endif ( DUSK_USE_NVAPI )

if ( DUSK_USE_AGS )
    include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/ags/include" )    
    target_link_libraries( ${TargetName} 
                           debug "${DUSK_BASE_FOLDER}Dusk/ThirdParty/ags/lib/amd_ags_x64_2019_MDd.lib" 
                           optimized "${DUSK_BASE_FOLDER}Dusk/ThirdParty/ags/lib/amd_ags_x64_2019_MD.lib" ) 
endif ( DUSK_USE_AGS )
endMacro()
