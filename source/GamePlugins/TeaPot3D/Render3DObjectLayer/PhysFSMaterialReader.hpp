#pragma once
#include <tiny_obj_loader.h>

#include <Layers/PhysFSLayer.h>

#include <cstddef>
#include <format>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// PhysFSMaterialReader
// Resolves .mtl paths via PhysFSLayer so tinyobjloader never touches the real
// filesystem.  BaseDir is the virtual directory of the .obj file, e.g.
//   "assets/models/"
//
// Header-only: LoadMtl() is only defined in the translation unit that defines
// TINYOBJLOADER_IMPLEMENTATION (Render3DObjectLayer.cpp), which is also the
// only TU that instantiates this reader — so this header must not be included
// from any other TU.
// ─────────────────────────────────────────────────────────────────────────────
class PhysFSMaterialReader : public tinyobj::MaterialReader
{
public:
    PhysFSMaterialReader(PhysFSLayer& Physfs, std::string BaseDir)
        : m_physfs(Physfs), m_baseDir(std::move(BaseDir)) {}

    inline bool operator()(const std::string& MatId,
                           std::vector<tinyobj::material_t>* Materials,
                           std::map<std::string, int>*        MatMap,
                           std::string*                       Warn,
                           std::string*                       Err) override
    {
        std::string path = m_baseDir + MatId;
        auto bytes = m_physfs.ReadFile(path.c_str());
        if (bytes.empty())
        {
            if (Warn) *Warn += std::format("[Render3D] MTL not found in VFS: '{}'\n", path);
            return false;
        }

        std::string        text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::istringstream ss(text);
        tinyobj::LoadMtl(MatMap, Materials, &ss, Warn, Err);
        return true;
    }

private:
    PhysFSLayer& m_physfs;
    std::string  m_baseDir;
};

