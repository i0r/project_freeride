file(GLOB_RECURSE SRC "${DUSK_BASE_FOLDER}DuskCore/Core/*.cpp"
    "${DUSK_BASE_FOLDER}DuskCore/FileSystem/*.cpp"
    "${DUSK_BASE_FOLDER}DuskCore/Framework/*.cpp"
    "${DUSK_BASE_FOLDER}DuskCore/Input/*.cpp"
    "${DUSK_BASE_FOLDER}DuskCore/Io/*.cpp"
    "${DUSK_BASE_FOLDER}DuskCore/Maths/*.cpp"
    "${DUSK_BASE_FOLDER}DuskCore/Rendering/*.cpp"
    "${DUSK_BASE_FOLDER}DuskCore/ThirdParty/*.cpp"
    "${DUSK_BASE_FOLDER}DuskCore/ThirdParty/*.c" )
file(GLOB_RECURSE INC "${DUSK_BASE_FOLDER}DuskCore/Core/*.h"
    "${DUSK_BASE_FOLDER}DuskCore/Core/*.inl"
    "${DUSK_BASE_FOLDER}DuskCore/FileSystem/*.h"
    "${DUSK_BASE_FOLDER}DuskCore/Framework/*.h"
    "${DUSK_BASE_FOLDER}DuskCore/Input/*.h"
    "${DUSK_BASE_FOLDER}DuskCore/Io/*.h"
    "${DUSK_BASE_FOLDER}DuskCore/Maths/*.h"
    "${DUSK_BASE_FOLDER}DuskCore/Maths/*.inl"
    "${DUSK_BASE_FOLDER}DuskCore/Rendering/*.h"
    "${DUSK_BASE_FOLDER}DuskCore/ThirdParty/*.h"
    "${DUSK_BASE_FOLDER}DuskCore/ThirdParty/*.hpp"
    "${DUSK_BASE_FOLDER}DuskCore/ThirdParty/*.inl")

set( SOURCES ${SRC} ${INC} "${DUSK_BASE_FOLDER}DuskCore/Shared.cpp" "${DUSK_BASE_FOLDER}DuskCore/Shared.h" )
add_precompiled_header( "${SOURCES}" Shared.h )

if ( ${DUSK_USE_UNITY_BUILD} )
	enable_unity_build( DuskCore SRC 16 cpp)
endif ( ${DUSK_USE_UNITY_BUILD} )

set( SOURCES ${SRC} ${INC} "${DUSK_BASE_FOLDER}DuskCore/Shared.cpp" "${DUSK_BASE_FOLDER}DuskCore/Shared.h" )

add_library( DuskCore STATIC ${SOURCES} )

include_directories( "${DUSK_BASE_FOLDER}DuskCore/ThirdParty" )
include_directories( "${DUSK_BASE_FOLDER}DuskCore" )

if(MSVC)
  target_compile_options(DuskCore PRIVATE /W3 /WX)
else()
  target_compile_options(DuskCore PRIVATE -Wall -Wextra)
endif()

add_msvc_filters( "${SOURCES}" )

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET DuskCore
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}DuskCore/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
