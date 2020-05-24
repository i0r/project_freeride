/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

namespace dk
{
    namespace core
    {
        enum class SelectionType {
            OpenFile,
            SaveFile,
        };

        // Display a platform specific modal window to select a file to open/save.
        // Return true if a file has been selected (with selectedFilename as the selected filename).
        // Note filetypeFilter should be formated in the crappy WinAPI way (e.g. MyFormat\0.myFormatExt\0MyFormat2 etc.).
        bool DisplayFileSelectionPrompt( dkString_t& selectedFilename,
                                    const SelectionType selectionType = SelectionType::OpenFile,
                                    const dkChar_t* filetypeFilter = DUSK_STRING( "*.*" ), 
                                    const dkString_t& initialDirectory = DUSK_STRING( "./" ),
                                    const dkString_t& promptTitle = DUSK_STRING( "Open/Save File" ) );
    }
}
