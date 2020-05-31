file(GLOB SRC 
    "${DUSK_BASE_FOLDER}DuskLexer/*.cpp" )
file(GLOB INC 
    "${DUSK_BASE_FOLDER}DuskLexer/*.h")

set( SOURCES ${SRC} ${INC} )

if ( ${DUSK_USE_UNITY_BUILD} )
	enable_unity_build( DuskLexer SRC 16 cpp)
endif ( ${DUSK_USE_UNITY_BUILD} )

set( SOURCES ${SRC} ${INC} )

add_library( DuskLexer STATIC ${SOURCES} )

set_property(TARGET DuskLexer PROPERTY FOLDER "DuskEngine")

# Should be removed in the future (only required for shader stage enum...)
include_directories( "${DUSK_BASE_FOLDER}DuskRenderer" )
include_directories( "${DUSK_BASE_FOLDER}DuskCore" )

if(MSVC)
  target_compile_options(DuskLexer PRIVATE /W3 /WX)
else()
  target_compile_options(DuskLexer PRIVATE -Wall -Wextra)
endif()

add_msvc_filters( "${SOURCES}" )

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET DuskLexer
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}DuskLexer/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
