/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_WIN
#include "AllocationHelpersWin32.h"
#elif DUSK_UNIX
#include "AllocationHelpersUnix.h"
#endif

namespace dk
{
    namespace core
    {
        DUSK_INLINE static void* AlignForward( void* address, const u8 alignment )
        {
            return reinterpret_cast< void* >(
                static_cast< u64 >( ( reinterpret_cast< u64 >( address ) + static_cast< u8 >( alignment - 1 ) ) )
                & static_cast< u8 >( ~( alignment - 1 ) )
            );
        }

        DUSK_INLINE static u8 AlignForwardAdjustment( const void* address, u8 alignment )
        {
            u8 adjustment = alignment - ( reinterpret_cast< u64 >( address )
                                          & static_cast< u8 >( alignment - 1 ) );

            return ( adjustment == alignment ) ? 0 : adjustment;
        }

        DUSK_INLINE static u8 AlignForwardAdjustmentWithHeader( const void* address, const u8 alignment, const u8 headerSize )
        {
            u8 adjustment = AlignForwardAdjustment( address, alignment );
            u8 neededSpace = headerSize;

            if ( adjustment < neededSpace ) {
                neededSpace -= adjustment;

                // Increase adjustment to fit header 
                adjustment += alignment * ( neededSpace / alignment );

                if ( neededSpace % alignment > 0 ) {
                    adjustment += alignment;
                }
            }

            return adjustment;
        }
    }
}
