/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BakerCache
{
public:
            BakerCache();
            ~BakerCache();

    // Load and store in-memory cache content from a given stream.
    void    load( FileSystemObject* stream );

    // Write to a given stream the in-memory content of the cache.
    void    store( FileSystemObject* stream );

    // Return the content hashcode for a given key (if the key exists
    // and is valid); return 0 if the key is unknown/invalid.
    u32     getContentHashcode( const dkStringHash_t key ) const;

    // Update a cache entry if its key already exist or create it.
    void    updateOrCreateEntry( const dkStringHash_t key, const u32 value );

    // Return true if the content hashcode for the given key is different to the new value provided;
    // (which mean the content has changed); false otherwise.
    bool    isEntryDirty( const dkStringHash_t key, const u32 newValue ) const;

private:
    // Hashmap for the baker cache. Key is the render library hashcode,
    // value is its content hashcode.
    std::unordered_map<dkStringHash_t, u32>     cacheData;
};
