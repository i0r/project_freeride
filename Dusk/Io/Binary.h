/*
    Project Motorway Source Code
    Copyright (C) 2018 Prévost Baptiste

    This file is part of Project Motorway source code.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once

#include <vector>
#include <FileSystem/FileSystemObject.h>

namespace dk
{
    namespace io
    {
        // Helper to load a raw binary file into a vector
        DUSK_INLINE static void LoadBinaryFile( FileSystemObject* stream, std::vector<u8>& bytesVector )
        {
            const u64 contentLength = stream->getSize();
            bytesVector.resize( contentLength, '\0' );

            stream->read( const_cast<u8*>( bytesVector.data() ), contentLength * sizeof( u8 ) );
        }
    }
}
