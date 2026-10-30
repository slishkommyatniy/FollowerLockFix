#include "pch.h"

namespace FollowerLockFix
{
    using FnActivateRef = bool (*)(
        RE::TESObjectREFR* a_this,
        RE::TESObjectREFR* a_activator,
        std::uint8_t a_arg2,
        RE::TESBoundObject* a_object,
        std::int32_t a_count,
        bool a_defaultProcessingOnly);

    static inline FnActivateRef _originalActivateRef = nullptr;

    bool Hooked_ActivateRef(
        RE::TESObjectREFR* a_this,
        RE::TESObjectREFR* a_activator,
        std::uint8_t a_arg2,
        RE::TESBoundObject* a_object,
        std::int32_t a_count,
        bool a_defaultProcessingOnly)
    {
        if (a_this && a_activator) {
            // Если объект заперт (сундук, сейф, дверь)
            if (a_this->IsLocked()) {
                auto* player = RE::PlayerCharacter::GetSingleton();

                // Если открывает НЕ игрок (спутник/NPC по приказу)
                if (a_activator != player) {
                    auto* actor = a_activator->As<RE::Actor>();
                    if (actor) {
                        auto* lock = a_this->GetLock();
                        if (lock) {
                            auto* key = lock->key;
                            auto lockLevel = a_this->GetLockLevel();
                            auto inv = actor->GetInventory();

                            // 1. Замок требует ключ
                            if (lockLevel == RE::LOCK_LEVEL::kRequiresKey || key != nullptr) {
                                if (!key) {
                                    return false; // Ключа вообще нет в игре — замок заблокирован
                                }
                                auto it = inv.find(key);
                                if (it == inv.end() || it->second.first <= 0) {
                                    return false; // У спутника нет ключа — запрещаем открывать!
                                }
                            }
                            // 2. Обычный замок (Новичок, Ученик, Мастер и т.д.)
                            else {
                                auto* lockpick = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000A);
                                if (lockpick) {
                                    auto it = inv.find(lockpick);
                                    if (it == inv.end() || it->second.first <= 0) {
                                        return false; // У спутника нет отмычек — запрещаем взлом!
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        return _originalActivateRef(a_this, a_activator, a_arg2, a_object, a_count, a_defaultProcessingOnly);
    }

    void Install()
    {
        auto& trampoline = SKSE::GetTrampoline();
        trampoline.create(14);

        _originalActivateRef = reinterpret_cast<FnActivateRef>(
            trampoline.write_branch<5>(
                REL::RelocationID(19369, 19796).address(),
                Hooked_ActivateRef));
    }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            FollowerLockFix::Install();
        }
    });

    return true;
}
