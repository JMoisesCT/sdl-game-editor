#include "components/AudioComponents.h"

#include "core/Reflection.h"
#include "systems/Conditions.h"

void registerAudioComponents(ComponentRegistry& registry, const ConditionRegistry& conditions) {
    const ConditionRegistry* conds = &conditions; // vive en Application, como el registro
    registry.add<SoundEffects>("SoundEffects")
        .list("sounds", &SoundEffects::sounds, [conds](FieldsBuilder<SoundRule>& rule) {
            rule.field("condition", &SoundRule::condition)
                    .choices([conds](const World&, Entity) { return conds->names(); })
                    .tooltip("Suena cuando esta condicion EMPIEZA a cumplirse")
                .field("sound", &SoundRule::sound).audio()
                .field("volume", &SoundRule::volume).range(0.0f, 2.0f).speed(0.01f);
        });

    registry.add<Music>("Music")
        .field("music", &Music::music).audio()
        .field("volume", &Music::volume).range(0.0f, 2.0f).speed(0.01f)
        .field("loop", &Music::loop).tooltip("Al terminar vuelve a empezar");
}
