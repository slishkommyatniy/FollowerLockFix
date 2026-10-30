#include "pch.h"

namespace FollowerLockFix
{
    bool ShouldBlockFavor(RE::Actor* a_activator, RE::TESObjectREFR* a_toActivate)
    {
        if (!a_activator || !a_toActivate) {
            return false;
        }

        // Фикс работает ТОЛЬКО для спутников игрока.
        // Обычные жители городов, стражники и враги не проверяются!
        if (!a_activator->IsPlayerTeammate()) {
            return false;
        }

        if (!a_toActivate->IsLocked()) {
            return false;
        }

        auto* lock = a_toActivate->GetLock();
        if (!lock) {
            return false;
        }

        auto* key = lock->key;
        auto lockLevel = a_toActivate->GetLockLevel();
        auto inv = a_activator->GetInventory();

        // 1. Если у спутника в кармане есть ключ от этого замка — открывает свободно
        if (key) {
            auto itKey = inv.find(key);
            if (itKey != inv.end() && itKey->second.first > 0) {
                return false;
            }
        }

        // 2. Если замок ТРЕБУЕТ КЛЮЧ — без ключа взломать невозможно
        if (lockLevel == RE::LOCK_LEVEL::kRequiresKey) {
            return true;
        }

        // 3. Обычный замок (Новичок .. Мастер):
        // Проверяем наличие отмычек у спутника
        auto* lockpick = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000A);
        if (lockpick) {
            auto it = inv.find(lockpick);
            if (it == inv.end() || it->second.first <= 0) {
                return true; // Нет отмычек — нечем ковырять замок!
            }
        }

        // Проверяем навык взлома спутника (учитывает статы NFF)
        float lockpickingSkill = 0.0f;
        auto* avOwner = a_activator->AsActorValueOwner();
        if (avOwner) {
            lockpickingSkill = avOwner->GetActorValue(RE::ActorValue::kLockpicking);
        }

        float requiredSkill = 0.0f;
        switch (lockLevel) {
        case RE::LOCK_LEVEL::kVeryEasy:  // Новичок
            requiredSkill = 0.0f;
            break;
        case RE::LOCK_LEVEL::kEasy:      // Ученик
            requiredSkill = 25.0f;
            break;
        case RE::LOCK_LEVEL::kAverage:   // Адепт
            requiredSkill = 50.0f;
            break;
        case RE::LOCK_LEVEL::kHard:      // Эксперт
            requiredSkill = 75.0f;
            break;
        case RE::LOCK_LEVEL::kVeryHard:  // Мастер
            requiredSkill = 100.0f;
            break;
        default:
            break;
        }

        if (lockpickingSkill < requiredSkill) {
            return true; // Навыка недостаточно — спутник отказывается!
        }

        return false; // Есть отмычка и хватает навыка — разрешено!
    }

    // --- CalculateDoFavor (VTable 0x4D) ---
    using FnCalculateDoFavor = bool (*)(
        RE::TESBoundObject* a_this,
        RE::Actor* a_activator,
        bool a_arg2,
        RE::TESObjectREFR* a_toActivate,
        float a_arg3);

    static inline FnCalculateDoFavor _originalCalculateDoFavorCONT = nullptr;
    static inline FnCalculateDoFavor _originalCalculateDoFavorDOOR = nullptr;

    bool Hooked_CalculateDoFavorCONT(
        RE::TESBoundObject* a_this,
        RE::Actor* a_activator,
        bool a_arg2,
        RE::TESObjectREFR* a_toActivate,
        float a_arg3)
    {
        if (ShouldBlockFavor(a_activator, a_toActivate)) {
            return false;
        }
        return _originalCalculateDoFavorCONT(a_this, a_activator, a_arg2, a_toActivate, a_arg3);
    }

    bool Hooked_CalculateDoFavorDOOR(
        RE::TESBoundObject* a_this,
        RE::Actor* a_activator,
        bool a_arg2,
        RE::TESObjectREFR* a_toActivate,
        float a_arg3)
    {
        if (ShouldBlockFavor(a_activator, a_toActivate)) {
            return false;
        }
        return _originalCalculateDoFavorDOOR(a_this, a_activator, a_arg2, a_toActivate, a_arg3);
    }

    // --- Activate (VTable 0x37) только для сундуков ---
    using FnActivate = bool (*)(
        RE::TESBoundObject* a_this,
        RE::TESObjectREFR* a_targetRef,
        RE::TESObjectREFR* a_activatorRef,
        std::uint8_t a_arg3,
        RE::TESBoundObject* a_object,
        std::int32_t a_targetCount);

    static inline FnActivate _originalActivateCONT = nullptr;

    bool Hooked_ActivateCONT(
        RE::TESBoundObject* a_this,
        RE::TESObjectREFR* a_targetRef,
        RE::TESObjectREFR* a_activatorRef,
        std::uint8_t a_arg3,
        RE::TESBoundObject* a_object,
        std::int32_t a_targetCount)
    {
        auto* actor = a_activatorRef ? a_activatorRef->As<RE::Actor>() : nullptr;
        if (actor && ShouldBlockFavor(actor, a_targetRef)) {
            return false;
        }
        return _originalActivateCONT(a_this, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount);
    }

    void Install()
    {
        // Перехватываем сундуки
        REL::Relocation<std::uintptr_t> contVtbl{ RE::VTABLE_TESObjectCONT[0] };
        _originalActivateCONT = reinterpret_cast<FnActivate>(contVtbl.write_vfunc(0x37, Hooked_ActivateCONT));
        _originalCalculateDoFavorCONT = reinterpret_cast<FnCalculateDoFavor>(contVtbl.write_vfunc(0x4D, Hooked_CalculateDoFavorCONT));

        // Перехватываем приказы для дверей (без вторжения в физическую активацию NPC)
        REL::Relocation<std::uintptr_t> doorVtbl{ RE::VTABLE_TESObjectDOOR[0] };
        _originalCalculateDoFavorDOOR = reinterpret_cast<FnCalculateDoFavor>(doorVtbl.write_vfunc(0x4D, Hooked_CalculateDoFavorDOOR));
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
