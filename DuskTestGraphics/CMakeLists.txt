file(GLOB SRC 	"${DUSK_BASE_FOLDER}DuskTestGraphics/*.cpp" )
				
file(GLOB INC 	"${DUSK_BASE_FOLDER}DuskTestGraphics/*.h"
                "${DUSK_BASE_FOLDER}DuskTestGraphics/*.ico"
                "${DUSK_BASE_FOLDER}DuskTestGraphics/*.rc"
                "${DUSK_BASE_FOLDER}DuskTestGraphics/*.manifest" )

if ( DUSK_USE_UNITY_BUILD )
	enable_unity_build( DuskTestGraphics SRC 16 cpp)
endif()

set( SOURCES ${SRC} ${INC}  )

add_msvc_filters( "${SOURCES}" )

add_executable( DuskTestGraphics ${GUI_TYPE} ${SOURCES})

set_property(TARGET DuskTestGraphics PROPERTY FOLDER "Projects")

Dusk_UseRendering( DuskTestGraphics )
target_link_libraries( DuskTestGraphics debug Dusk_Debug optimized Dusk )

include_directories( "${DUSK_BASE_FOLDER}DuskTestGraphics" )
include_directories( "${DUSK_BASE_FOLDER}Dusk" )
include_directories( "${DUSK_BASE_FOLDER}Dusk/ThirdParty/" )

if ( WIN32 )
    target_link_libraries( DuskTestGraphics winmm Pathcch Shlwapi )
    set_target_properties( DuskTestGraphics PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS" )
elseif( UNIX )
    target_link_libraries( DuskTestGraphics dl X11 xcb xcb-keysyms X11-xcb ${X11_LIBRARIES} )
endif ( WIN32 )

if(MSVC)
  target_compile_options(DuskTestGraphics PRIVATE /W3 /WX)
else()
  target_compile_options(DuskTestGraphics PRIVATE -Wall -Wextra)
endif()

if ( UNIX )
    find_package(PkgConfig REQUIRED)
    find_package(Threads REQUIRED)

    PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

    include_directories(${GTK3_INCLUDE_DIRS})
    link_directories(${GTK3_LIBRARY_DIRS})

    add_definitions(${GTK3_CFLAGS_OTHER})

    set(THREADS_PREFER_PTHREAD_FLAG ON)

    target_link_libraries(DuskTestGraphics Threads::Threads)
    target_link_libraries(DuskTestGraphics ${GTK3_LIBRARIES})
endif( UNIX )

if ( DUSK_USE_UNITY_BUILD )
    if(MSVC)
        add_custom_command( TARGET DuskTestGraphics
            PRE_BUILD
            COMMAND RD /S /Q ${DUSK_BASE_FOLDER}DuskTestGraphics/UnityBuild/
        )
    endif()
endif ( DUSK_USE_UNITY_BUILD )
