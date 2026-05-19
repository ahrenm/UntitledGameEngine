#include <ServiceLocator.h>

// Single definition of the registry map.
// Because this TU is compiled only into UGECore.dll, every DLL that links
// UGECore resolves ServiceLocator::registry() to this one function, so they
// all share the same std::unordered_map instance.  If the body were inline in
// the header each DLL would get its own function-local static — the classic
// "split singleton" — causing Provide() calls from one DLL to be invisible to
// Get() calls from another.
std::unordered_map<std::type_index, void*>& ServiceLocator::registry()
{
    static std::unordered_map<std::type_index, void*> Instance;
    return Instance;
}

