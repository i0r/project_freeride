file(GLOB_RECURSE SRC "${DUSK_BASE_FOLDER}Dusk/Core/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/FileSystem/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Framework/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Input/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Io/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Maths/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Parsing/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Graphics/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Rendering/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/Physics/*.cpp"
    "${DUSK_BASE_FOLDER}DuskEngine.cpp")
    
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
    "${DUSK_BASE_FOLDER}Dusk/Physics/*.h"
    "${DUSK_BASE_FOLDER}DuskEngine.h")
    
file(GLOB_RECURSE SRC_THIRD_PARTY 
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/SPIRV-Cross/*.cpp"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/SPIRV-Cross/*.c"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/SPIRV-Cross/*.hpp"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/SPIRV-Cross/*.h"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/miniz/src/*.c"
    "${DUSK_BASE_FOLDER}Dusk/ThirdParty/miniz/src/*.h")
    
set( SOURCES ${SRC} ${INC} "${DUSK_BASE_FOLDER}Dusk/Shared.cpp" "${DUSK_BASE_FOLDER}Dusk/Shared.h" )
add_precompiled_header( "${SOURCES}" Shared.h )

if ( ${DUSK_USE_UNITY_BUILD} )
	enable_unity_build( Dusk SRC 16 cpp)
endif ( ${DUSK_USE_UNITY_BUILD} )

set( SOURCES ${SRC} ${INC} ${SRC_THIRD_PARTY} "${DUSK_BASE_FOLDER}Dusk/Shared.cpp" "${DUSK_BASE_FOLDER}Dusk/Shared.h" )

add_library( Dusk STATIC ${SOURCES} )

set_property(TARGET Dusk PROPERTY FOLDER "DuskEngine")

Dusk_UseRendering( Dusk )

include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty" )
include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/miniz" )

if ( ${DUSK_USE_DIRECTX_COMPILER} )
    include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/dxc/include" )
endif( ${DUSK_USE_DIRECTX_COMPILER} )

include_directories( "${DUSK_BASE_FOLDER}Dusk" )

if(MSVC)
  target_compile_options(Dusk PRIVATE /W3 /WX)
else()
  target_compile_options(Dusk PRIVATE -Wall -Wextra)
endif()

add_msvc_filters( "${SOURCES}" )

if ( ${DUSK_USE_DIRECTX_COMPILER} )
    if ( WIN32 )
        target_link_libraries( Dusk d3dcompiler )
    elseif( UNIX )
        target_link_libraries( Dusk "${DUSK_BASE_FOLDER}Dusk/ThirdParty/dxc/lib/libdxcompiler.so" )
    endif( WIN32 )
endif ( ${DUSK_USE_DIRECTX_COMPILER} )
  
if ( DUSK_USE_STB_IMAGE )
    include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/stb_image/include" )
endif ( DUSK_USE_STB_IMAGE )

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET Dusk
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}Dusk/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
