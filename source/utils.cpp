#include "../include/utils.h"

ARENA_ERROR Arena::reserveArena(u64 sizeInBytes){
    void* tempAddress = VirtualAlloc(NULL, sizeInBytes, MEM_RESERVE, PAGE_READWRITE);
    if (tempAddress == NULL)
        return ARENA_CREATION_FAILURE;
    // Batch write to the struct to avoid cache misses.
    arenaBasePointer = tempAddress;
    arenaSize = sizeInBytes;
    arenaSizeCommitted = 0;
    arenaSizeLeftReserved  = sizeInBytes;
    arenaSizeLeftInCommitted = 0;
    arenaLatestPointer = tempAddress;
    return ARENA_OK;
}

ARENA_ERROR Arena::commitArena(u64 sizeInBytes){
    if (sizeInBytes > arenaSizeLeftReserved)
        return ARENA_NO_SPACE_FOR_COMMIT;
    else {
        void* tempAddress = VirtualAlloc(arenaLatestPointer, sizeInBytes, MEM_COMMIT, PAGE_READWRITE);
        if (tempAddress == NULL)
            return ARENA_COMMIT_FAILURE;
        arenaSizeCommitted += sizeInBytes;
        arenaSizeLeftReserved  = arenaSizeLeftReserved - sizeInBytes;
        arenaSizeLeftInCommitted += sizeInBytes;
        return ARENA_OK;
    }
}

ARENA_ERROR Arena::insertData(void* data, u64 sizeInBytes, void** outMemoryPointer){
    if (sizeInBytes > arenaSizeLeftInCommitted)
        return ARENA_NO_SPACE_FOR_DATA;
    if (data != NULL)
        memcpy(arenaLatestPointer, data, sizeInBytes);
    byte* newAddress = static_cast<byte*>(arenaLatestPointer);
    newAddress += sizeInBytes;
    *outMemoryPointer = arenaLatestPointer;
    arenaLatestPointer = static_cast<void*>(newAddress);
    arenaSizeLeftInCommitted = arenaSizeLeftInCommitted - sizeInBytes;
    return ARENA_OK;
}

ARENA_ERROR Arena::removeData(u64 sizeInBytes){ // Since we want to pop items as a stack, we won't be deleting this from the middle anyways.
    if (sizeInBytes > arenaSizeCommitted) {
        return ARENA_CANT_DELETE_RESERVED;
    }
    if (sizeInBytes > (arenaSizeCommitted - arenaSizeLeftInCommitted))
        return ARENA_CANT_DELETE_NULL;
    byte* newArenaPointer = static_cast<byte*>(arenaLatestPointer) - sizeInBytes;
    arenaSizeLeftInCommitted = arenaSizeLeftInCommitted - sizeInBytes;
    arenaLatestPointer = static_cast<void*>(newArenaPointer);
    return ARENA_OK;
}

ARENA_ERROR Arena::removeArena(){
    BOOL temp = VirtualFree(arenaBasePointer, arenaSize, MEM_FREE);
    if (temp == FALSE)
        return ARENA_FREE_FAILURE;
    return ARENA_OK;
}

ARENA_ERROR Arena::pushPointer(u32 offsetInBytes){
    if(offsetInBytes > arenaSizeLeftInCommitted){
        return ARENA_NO_SPACE_FOR_DATA;
    }
    byte* newArenaPointer = static_cast<byte*>(arenaLatestPointer) + offsetInBytes;
    arenaLatestPointer = static_cast<void*>(newArenaPointer);
    return ARENA_OK;
}

ARENA_ERROR Arena::castPointer(void** outMemoryPointer){
    *outMemoryPointer = arenaLatestPointer;
    return ARENA_OK;
}
