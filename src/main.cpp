#include "pch.h"

namespace FollowerLockFix
{
    using FnActivate = bool (*)(
        RE::TESBoundObject* a_this,
        RE::TESObjectREFR* a_targetRef,
        RE::TESObjectREFR* a_activatorRef,
        std::uint8_t a_arg3,
        RE::TESBoundObject* a_object,
        std::int32_t a_targetCount);

    static inline FnActivate _originalActivate = nullptr;

    bool Hooked_Activate(
        RE::TESBoundObject* a_this,
        RE::TESObjectREFR* a_targetRef,
        RE::TESObjectREFR* a_activatorRef,
        std::uint8_t a_arg3,
        RE::TESBoundObject* a_object,
        std::int32_t a_targetCount)
    {
        if (a_targetRef && a_activatorRef) {
            // Если сундук/контейнер заперт
            if (a_targetRef->IsLocked()) {
                auto* player = RE::PlayerCharacter::GetSingleton();

                // Если открывает НЕ игрок (спутник по приказу)
                if (a_activatorRef != player) {
                    auto* actor = a_activatorRef->As<RE::Actor>();
                    if (actor) {
                        auto* lock = a_targetRef->GetLock();
                        if (lock) {
                            auto* key = lock->key;
                            auto lockLevel = a_targetRef->GetLockLevel();
                            auto inv = actor->GetInventory();

                            // 1. Замок требует ключ
                            if (lockLevel == RE::LOCK_LEVEL::kRequiresKey || key != nullptr) {
                                if (!key) {
                                    return false; // Замок заблокирован сюжетно
                                }
                                auto it = inv.find(key);
                                if (it == inv.end() || it->second.first <= 0) {
                                    return false; // У спутника нет ключа — запрещаем открывать!
                                }
                            }
                            // 2. Обычный замок (Новичок, Мастер и т.д.)
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

        return _originalActivate(a_this, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount);
    }

    void Install()
    {
        // Перехватываем виртуальную функцию Activate ТОЛЬКО у контейнеров (TESObjectCONT)
        REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_TESObjectCONT[0] };
        _originalActivate = reinterpret_cast<FnActivate>(vtbl.write_vfunc(0x37, Hooked_Activate));
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
