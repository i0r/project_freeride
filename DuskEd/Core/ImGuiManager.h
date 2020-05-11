/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_DEVBUILD
class DisplaySurface;

class ImGuiManager
{
public:
                                    ImGuiManager();
                                    ImGuiManager( ImGuiManager& ) = delete;
                                    ImGuiManager& operator = ( ImGuiManager& ) = delete;
                                    ~ImGuiManager();

    void                            create( const DisplaySurface& displaySurface );
    void                            update( const f32 deltaTime );

    void                            setVisible( const bool isVisible );

private:
    std::string                     settingsFilePath;
};
#endif
