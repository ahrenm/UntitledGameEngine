#include <PhysFSFileInterface.h>
#include <SDL3/SDL.h>

Rml::FileHandle PhysFSFileInterface::Open(const Rml::String& Path)
{
    PHYSFS_File* F = PHYSFS_openRead(Path.c_str());
    if (!F)
        SDL_Log("PhysFSFileInterface: cannot open '%s': %s", Path.c_str(),
                PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    return reinterpret_cast<Rml::FileHandle>(F);
}

void PhysFSFileInterface::Close(Rml::FileHandle File)
{
    if (File)
        PHYSFS_close(reinterpret_cast<PHYSFS_File*>(File));
}

size_t PhysFSFileInterface::Read(void* Buffer, size_t Size, Rml::FileHandle File)
{
    if (!File) return 0;
    const PHYSFS_sint64 N = PHYSFS_readBytes(reinterpret_cast<PHYSFS_File*>(File), Buffer,
                                              static_cast<PHYSFS_uint64>(Size));
    return N < 0 ? 0 : static_cast<size_t>(N);
}

bool PhysFSFileInterface::Seek(Rml::FileHandle File, long Offset, int Origin)
{
    if (!File) return false;
    auto* F = reinterpret_cast<PHYSFS_File*>(File);

    PHYSFS_sint64 Target = 0;
    if (Origin == SEEK_SET)
        Target = Offset;
    else if (Origin == SEEK_CUR)
        Target = PHYSFS_tell(F) + Offset;
    else if (Origin == SEEK_END)
        Target = PHYSFS_fileLength(F) + Offset;

    return PHYSFS_seek(F, static_cast<PHYSFS_uint64>(Target)) != 0;
}

size_t PhysFSFileInterface::Tell(Rml::FileHandle File)
{
    if (!File) return 0;
    const PHYSFS_sint64 Pos = PHYSFS_tell(reinterpret_cast<PHYSFS_File*>(File));
    return Pos < 0 ? 0 : static_cast<size_t>(Pos);
}

size_t PhysFSFileInterface::Length(Rml::FileHandle File)
{
    if (!File) return 0;
    const PHYSFS_sint64 Len = PHYSFS_fileLength(reinterpret_cast<PHYSFS_File*>(File));
    return Len < 0 ? 0 : static_cast<size_t>(Len);
}
