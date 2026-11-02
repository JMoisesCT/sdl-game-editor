#include "core/Reflection.h"

bool ComponentInfo::has(const World& w, Entity e) const {
    const IComponentPool* p = w.findPool(typeId);
    return p && p->has(e);
}

void* ComponentInfo::get(World& w, Entity e) const {
    IComponentPool* p = w.findPool(typeId);
    return p ? p->getRaw(e) : nullptr;
}

const void* ComponentInfo::get(const World& w, Entity e) const {
    const IComponentPool* p = w.findPool(typeId);
    return p ? p->getRaw(e) : nullptr;
}

void ComponentInfo::remove(World& w, Entity e) const {
    if (IComponentPool* p = w.findPool(typeId)) p->remove(e);
}

const ComponentInfo* ComponentRegistry::find(const std::string& name) const {
    for (const auto& info : infos)
        if (info->name == name) return info.get();
    return nullptr;
}

const ComponentInfo* ComponentRegistry::findByTypeId(std::size_t typeId) const {
    for (const auto& info : infos)
        if (info->typeId == typeId) return info.get();
    return nullptr;
}
