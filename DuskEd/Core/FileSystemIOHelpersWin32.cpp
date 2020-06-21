/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Dusk/Shared.h>

#if DUSK_WIN
#include "FileSystemIOHelpers.h"

#include <commdlg.h>

bool dk::core::DisplayFileSelectionPrompt( dkString_t& selectedFilename, 
                                           const SelectionType selectionType /*= SelectionType::OpenFile*/, 
                                           const dkChar_t* filetypeFilter /*= DUSK_STRING( "*.*" )*/, 
                                           const dkString_t& initialDirectory /*= DUSK_STRING( "./" )*/, 
                                           const dkString_t& promptTitle /*= DUSK_STRING( "Open/Save File" ) */ )
{
    selectedFilename.resize( MAX_PATH );

    OPENFILENAMEW ofn = {};
    HANDLE hf = {};

    ZeroMemory( &ofn, sizeof( ofn ) );
    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = &selectedFilename[0];

    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
    // use the contents of szFile to initialize itself.
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filetypeFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = initialDirectory.c_str();
    ofn.lpstrTitle = promptTitle.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Display the Open dialog box.
    return ( selectionType == SelectionType::OpenFile ) ? GetOpenFileName( &ofn ) : GetSaveFileName( &ofn );
}
#endif
