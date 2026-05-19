#pragma once

#include <RmlUi/Core/FileInterface.h>
#include <physfs.h>

/// Implements Rml::FileInterface using PhysicsFS so that RmlUi
/// reads all files (rml, rcss, fonts) via the virtual filesystem.
class PhysFSFileInterface : public Rml::FileInterface
{
public:
    Rml::FileHandle Open(const Rml::String& Path) override;
    void            Close(Rml::FileHandle File) override;
    size_t          Read(void* Buffer, size_t Size, Rml::FileHandle File) override;
    bool            Seek(Rml::FileHandle File, long Offset, int Origin) override;
    size_t          Tell(Rml::FileHandle File) override;
    size_t          Length(Rml::FileHandle File) override;
};
