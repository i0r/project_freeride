file(GLOB SRC 	"${DUSK_BASE_FOLDER}DuskEd/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/Core/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/Core/Parsers/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/Framework/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/Framework/EditorWidgets/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/Framework/Transaction/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/Graphics/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/Graphics/RenderModules/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/FileSystem/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/Parsing/*.cpp"
                "${DUSK_BASE_FOLDER}DuskEd/*.inl"
                "${DUSK_BASE_FOLDER}DuskEd/Core/*.inl"
                "${DUSK_BASE_FOLDER}DuskEd/Framework/*.inl"
                "${DUSK_BASE_FOLDER}DuskEd/Graphics/*.inl" )
				
file(GLOB INC 	"${DUSK_BASE_FOLDER}DuskEd/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/Core/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/Core/Parsers/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/Framework/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/Framework/EditorWidgets/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/Framework/Transaction/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/Graphics/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/Graphics/RenderModules/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/FileSystem/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/Parsing/*.h"
                "${DUSK_BASE_FOLDER}DuskEd/*.ico"
                "${DUSK_BASE_FOLDER}DuskEd/*.rc"
                "${DUSK_BASE_FOLDER}DuskEd/*.manifest" )

if ( DUSK_USE_IMGUI )
	add_definitions( -DDUSK_USE_IMGUI )
    file(GLOB_RECURSE IMGUI "${DUSK_BASE_FOLDER}DuskEd/ThirdParty/imgui/*.cpp"
                            "${DUSK_BASE_FOLDER}DuskEd/ThirdParty/imgui/*.h"
                            "${DUSK_BASE_FOLDER}DuskEd/ThirdParty/ImGuizmo/*.cpp"
                            "${DUSK_BASE_FOLDER}DuskEd/ThirdParty/ImGuizmo/*.h" )
							
    include_directories( "${DUSK_BASE_FOLDER}DuskEd/ThirdParty/imgui" ) #ImGuizmo expects imgui to be in the include path
endif()

if ( DUSK_USE_RENDERDOC )
	add_definitions( -DDUSK_USE_RENDERDOC )
    file(GLOB_RECURSE RENDERDOC "${DUSK_BASE_FOLDER}DuskEd/ThirdParty/RenderDoc/*.h" )
endif()

if ( DUSK_USE_UNITY_BUILD )
	enable_unity_build( DuskEd SRC 16 cpp)
endif()

set( SOURCES ${SRC} ${INC} ${IMGUI} ${RENDERDOC} )

add_msvc_filters( "${SOURCES}" )

add_executable( DuskEd ${GUI_TYPE} ${SOURCES})

target_compile_definitions( Dusk PRIVATE DUSKED )
target_compile_definitions( DuskEd PRIVATE DUSKED )

set_property(TARGET DuskEd PROPERTY FOLDER "Projects")

target_link_libraries( DuskEd debug Dusk_Debug optimized Dusk )

if ( "${DUSK_GFX_API}" MATCHES "DUSK_D3D11" )
    target_link_libraries( DuskEd d3d11 )
    target_link_libraries( DuskEd dxgi )
    target_link_libraries( DuskEd d3dcompiler )
    target_link_libraries( DuskEd dxguid )
endif ( "${DUSK_GFX_API}" MATCHES "DUSK_D3D11" )

if ( DUSK_USE_FBXSDK )
    find_package( FBXSDK REQUIRED )
    include_directories( ${FBXSDK_INCLUDE_DIR} )
    
    target_link_libraries( DuskEd debug ${FBXSDK_LIBRARY_DEBUG} optimized ${FBXSDK_LIBRARY} )
    target_link_libraries( DuskEd debug ${LIBXML2_LIBRARY_DEBUG} optimized ${LIBXML2_LIBRARY} )
    target_link_libraries( DuskEd debug ${ZLIB_LIBRARY_DEBUG} optimized ${ZLIB_LIBRARY} )
endif( DUSK_USE_FBXSDK )

if ( DUSK_USE_NVAPI )
    target_link_libraries( DuskEd "${DUSK_BASE_FOLDER}Dusk/ThirdParty/nvapi/amd64/nvapi64.lib" ) 
endif ( DUSK_USE_NVAPI )

if ( DUSK_USE_AGS )
target_link_libraries( DuskEd 
                        debug "${DUSK_BASE_FOLDER}Dusk/ThirdParty/ags/lib/amd_ags_x64_2019_MDd.lib" 
                        optimized "${DUSK_BASE_FOLDER}Dusk/ThirdParty/ags/lib/amd_ags_x64_2019_MD.lib" ) 
endif ( DUSK_USE_AGS )

if ( DUSK_USE_BULLET )
include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/bullet3/src" )

target_link_libraries( DuskEd 
                        debug "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/Debug/BulletDynamics_Debug.lib" 
                        optimized "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/RelWithDebInfo/BulletDynamics_RelWithDebugInfo.lib" ) 
                        
target_link_libraries( DuskEd 
                        debug "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/Debug/BulletCollision_Debug.lib" 
                        optimized "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/RelWithDebInfo/BulletCollision_RelWithDebugInfo.lib" ) 
                        
target_link_libraries( DuskEd 
                        debug "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/Debug/LinearMath_Debug.lib" 
                        optimized "${DUSK_BASE_FOLDER}build/lib/bullet3/lib/RelWithDebInfo/LinearMath_RelWithDebugInfo.lib" ) 
endif ( DUSK_USE_BULLET )

include_directories( "${DUSK_BASE_FOLDER}DuskEd" )
include_directories( "${DUSK_BASE_FOLDER}Dusk" )
include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/" )
include_directories( "${DUSK_BASE_FOLDER}DuskEd/ThirdParty" )

if ( WIN32 )
    target_link_libraries( DuskEd winmm Pathcch Shlwapi )
    set_target_properties( DuskEd PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS" )
elseif( UNIX )
    target_link_libraries( DuskEd dl X11 xcb xcb-keysyms X11-xcb ${X11_LIBRARIES} )
endif ( WIN32 )

if(MSVC)
  target_compile_options(DuskEd PRIVATE /W3 /WX)
else()
  target_compile_options(DuskEd PRIVATE -Wall -Wextra)
endif()

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET DuskEd
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}DuskEd/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
