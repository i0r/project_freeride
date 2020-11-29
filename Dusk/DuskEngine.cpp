/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "DuskEngine.h"

DuskEngine* g_DuskEngine = nullptr;

DuskEngine::DuskEngine()
    : applicationName( DUSK_STRING( "DuskEngine" ) )
{
    DUSK_RAISE_FATAL_ERROR( ( g_DuskEngine == nullptr ), "A game engine instance has already been created!" );
}

DuskEngine::~DuskEngine()
{
    g_DuskEngine = nullptr;
}

void DuskEngine::create( DuskEngine::Parameters creationParameters, const char* cmdLineArgs )
{
    setApplicationName( creationParameters.ApplicationName );

    Timer profileTimer;
    profileTimer.start();
    DUSK_LOG_RAW( "================================\n%s %s\n%hs\nCompiled with: %s\n================================\n\n", applicationName.c_str(), DUSK_BUILD, DUSK_BUILD_DATE, DUSK_COMPILER );

    DUSK_LOG_INFO( "Initialization done (took %.5f seconds)\n", profileTimer.getElapsedTimeAsSeconds() );
    DUSK_LOG_RAW( "\n================================\n\n" );
}

void DuskEngine::shutdown()
{

}

void DuskEngine::setApplicationName( const dkChar_t* name /*= DUSK_STRING( "Dusk GameEngine" ) */ )
{
    if ( name == nullptr ) {
        return;
    }

    applicationName = dkString_t( name );
}

void DuskEngine::mainLoop()
{

}
