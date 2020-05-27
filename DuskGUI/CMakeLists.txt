file(GLOB_RECURSE SRC 
    "${DUSK_BASE_FOLDER}DuskGUI/Widgets/*.cpp" )
file(GLOB_RECURSE INC 
    "${DUSK_BASE_FOLDER}DuskGUI/Widgets/*.h")

set( SOURCES ${SRC} ${INC} )

if ( ${DUSK_USE_UNITY_BUILD} )
	enable_unity_build( DuskGUI SRC 16 cpp)
endif ( ${DUSK_USE_UNITY_BUILD} )

set( SOURCES ${SRC} ${INC} )

add_library( DuskGUI STATIC ${SOURCES} )

set_property(TARGET DuskGUI PROPERTY FOLDER "DuskEngine")

include_directories( "${DUSK_BASE_FOLDER}DuskRenderer" )
include_directories( "${DUSK_BASE_FOLDER}DuskCore" )

if(MSVC)
  target_compile_options(DuskGUI PRIVATE /W3 /WX)
else()
  target_compile_options(DuskGUI PRIVATE -Wall -Wextra)
endif()

add_msvc_filters( "${SOURCES}" )

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET DuskGUI
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}DuskGUI/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
