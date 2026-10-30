#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

namespace FollowerLockFix
{
    struct ActivateHook
    {
        static bool Hooked_Activate(
            RE::TESObjectREFR* a_target,
            RE::TESObjectREFR* a_activatorRef,
            std::uint8_t a_arg3,
            RE::TESBoundObject* a_object,
            std::int32_t a_count,
            bool a_defaultProcessingOnly)
        {
            if (a_target && a_activatorRef) {
                // Если объект заперт (сундук, сейф, дверь)
                if (a_target->IsLocked()) {
                    auto* player = RE::PlayerCharacter::GetSingleton();

                    // Если открывает НЕ игрок (спутник/NPC по приказу)
                    if (a_activatorRef != player) {
                        auto* actor = a_activatorRef->As<RE::Actor>();
                        if (actor) {
                            const auto lockLevel = a_target->GetLockLevel();
                            auto* requiredKey = a_target->GetKey();

                            // 1. Замок требует ключ
                            if (lockLevel == RE::LOCK_LEVEL::kRequiresKey || requiredKey != nullptr) {
                                if (!requiredKey || actor->GetItemCount(requiredKey) <= 0) {
                                    // У спутника НЕТ ключа — блокируем открытие сундука!
                                    return false;
                                }
                            }
                            // 2. Обычный замок без ключа
                            else {
                                auto* lockpick = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000A);
                                if (lockpick && actor->GetItemCount(lockpick) <= 0) {
                                    // У спутника нет отмычек — блокируем взлом!
                                    return false;
                                }
                            }
                        }
                    }
                }
            }

            return _originalActivate(a_target, a_activatorRef, a_arg3, a_object, a_count, a_defaultProcessingOnly);
        }

        static inline REL::Relocation<decltype(Hooked_Activate)> _originalActivate;

        static void Install()
        {
            REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_TESObjectREFR[0] };
            _originalActivate = vtbl.write_vfunc(0x37, Hooked_Activate);
        }
    };
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            FollowerLockFix::ActivateHook::Install();
        }
    });

    return true;
}
