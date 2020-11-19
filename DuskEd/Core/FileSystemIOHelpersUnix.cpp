/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Dusk/Shared.h>

#if DUSK_UNIX
#include "FileSystemIOHelpers.h"

#include <gtk/gtk.h>

bool dk::core::DisplayFileSelectionPrompt( dkString_t& selectedFilename, 
                                           const SelectionType selectionType /*= SelectionType::OpenFile*/, 
                                           const dkChar_t* filetypeFilter /*= DUSK_STRING( "*.*" )*/, 
                                           const dkString_t& initialDirectory /*= DUSK_STRING( "./" )*/, 
                                           const dkString_t& promptTitle /*= DUSK_STRING( "Open/Save File" ) */ )
{
    gtk_init_check(nullptr, nullptr);

    bool isSaving = false;

    const char* acceptText;
    GtkFileChooserAction gtkAction;
    switch (selectionType) {
    case SelectionType::OpenFile:
        gtkAction = GTK_FILE_CHOOSER_ACTION_OPEN;
        acceptText = "Open";
        break;
    case SelectionType::SaveFile:
        gtkAction = GTK_FILE_CHOOSER_ACTION_SAVE;
        acceptText = "Save";
        isSaving = true;
        break;
    default:
        DUSK_ASSERT( false, "Invalid Usage!" );
        break;
    }

    // Create the window.
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
                          promptTitle.c_str(),
                          NULL,
                          gtkAction,
                          "_Cancel", GTK_RESPONSE_CANCEL,
                          acceptText, GTK_RESPONSE_ACCEPT,
                          NULL);

    // Split WINAPI formated filter list.
    // TODO Might worth switching to an API independant format (since WINAPI format is terrible and counter intuitive).
    char* arg = strtok( const_cast<char*>( filetypeFilter ), "\0" );
    while ( arg != nullptr ) {
        const char* filterName = arg;
        arg = strtok( nullptr, "\0" );
        const char* filterPattern = arg;

        // Fill GTK filter descriptor.
        GtkFileFilter* fileFilter = gtk_file_filter_new();
        gtk_file_filter_set_name(fileFilter, filterName);
        gtk_file_filter_add_pattern(fileFilter, filterPattern);

        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), fileFilter);

        arg = strtok( nullptr, "\0" );
    }

    if (isSaving) {
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    }

    if (!initialDirectory.empty()) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), initialDirectory.c_str());
    }

    // Retrieve selected filename.
    char* filename = NULL;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }
    gtk_widget_destroy(dialog);
    selectedFilename = dkString_t(filename);

    // Flush pending events.
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    return !selectedFilename.empty();
}
#endif
