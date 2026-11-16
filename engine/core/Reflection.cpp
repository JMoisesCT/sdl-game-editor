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

static void visitEntityFields(const std::vector<FieldInfo>& fields, void* object,
                              const std::function<void(Entity&)>& fn) {
    for (const FieldInfo& f : fields) {
        if (f.type == FieldType::Entity) {
            fn(*static_cast<Entity*>(f.in(object)));
        } else if (f.type == FieldType::List) {
            void* list = f.in(object);
            const std::size_t n = f.listSize(list);
            for (std::size_t i = 0; i < n; ++i) visitEntityFields(f.elementFields, f.listElement(list, i), fn);
        }
    }
}

void forEachEntityField(const ComponentRegistry& registry, World& world, Entity e,
                        const std::function<void(Entity&)>& fn) {
    for (const auto& info : registry.all())
        if (void* c = info->get(world, e)) visitEntityFields(info->fields, c, fn);
}
