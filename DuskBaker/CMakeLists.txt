file(GLOB_RECURSE SRC "Baker.cpp" "EntryPointWin64.cpp" "EntryPointUnix.cpp" "CodeGenerator.cpp" "CodeGenerator.h" )
file(GLOB_RECURSE INC "Baker.h" )

if ( ${DUSK_USE_UNITY_BUILD} )
	enable_unity_build( DuskBaker SRC 16 cpp)
endif ( ${DUSK_USE_UNITY_BUILD} )

set( SOURCES ${SRC} ${INC} )

add_executable( DuskBaker ${SOURCES} )

set_property(TARGET DuskBaker PROPERTY FOLDER "Projects")

target_link_libraries( DuskBaker debug DuskCore_Debug optimized DuskCore )
target_link_libraries( DuskBaker debug DuskLexer_Debug optimized DuskLexer )

include_directories( "${DUSK_BASE_FOLDER}DuskBaker/ThirdParty" )
include_directories( "${DUSK_BASE_FOLDER}DuskBaker" )
include_directories( "${DUSK_BASE_FOLDER}DuskCore" )
include_directories( "${DUSK_BASE_FOLDER}DuskLexer" )
include_directories( "${DUSK_BASE_FOLDER}DuskRenderer" )

if ( WIN32 )
    target_link_libraries( DuskBaker winmm Pathcch Shlwapi )
    target_link_libraries( DuskBaker d3dcompiler )
elseif( UNIX )
    target_link_libraries( DuskBaker dl X11 xcb xcb-keysyms X11-xcb ${X11_LIBRARIES} )
endif ( WIN32 )

if(MSVC)
  target_compile_options(DuskBaker PRIVATE /W3 /WX)
else()
  target_compile_options(DuskBaker PRIVATE -Wall -Wextra)
endif()

add_msvc_filters( "${SOURCES}" )

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET DuskBaker
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}DuskBaker/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
