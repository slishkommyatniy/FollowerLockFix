#include "pch.h"
#include <spdlog/sinks/basic_file_sink_mt.h>

namespace FollowerLockFix
{
    static RE::TESBoundObject* g_lockpick = nullptr;

    bool ShouldBlockFavor(RE::Actor* a_activator, RE::TESObjectREFR* a_toActivate)
    {
        if (!a_toActivate || !a_toActivate->IsLocked()) {
            return false;
        }

        auto* lock = a_toActivate->GetLock();
        if (!lock) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (a_activator && a_activator == player) {
            return false;
        }

        auto* key = lock->key;
        auto lockLevel = a_toActivate->GetLockLevel();
        auto inv = a_activator ? a_activator->GetInventory() : decltype(a_activator->GetInventory()){};

        // 1. Замок требует ключ
        if (lockLevel == RE::LOCK_LEVEL::kRequiresKey || key != nullptr) {
            if (!key) {
                SKSE::log::info("Blocked: object 0x{:08X} requires key, but key is null", a_toActivate->GetFormID());
                return true;
            }
            if (!a_activator) {
                return true;
            }
            auto it = inv.find(key);
            if (it == inv.end() || it->second.first <= 0) {
                SKSE::log::info("Blocked: follower does not have key for 0x{:08X}", a_toActivate->GetFormID());
                return true;
            }
        }
        // 2. Обычный замок (Новичок .. Мастер)
        else {
            if (!a_activator) {
                return true;
            }
            if (g_lockpick) {
                auto it = inv.find(g_lockpick);
                if (it == inv.end() || it->second.first <= 0) {
                    SKSE::log::info("Blocked: follower has no lockpicks for 0x{:08X}", a_toActivate->GetFormID());
                    return true;
                }
            }
        }

        return false;
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

    // --- Activate (VTable 0x37) ---
    using FnActivate = bool (*)(
        RE::TESBoundObject* a_this,
        RE::TESObjectREFR* a_targetRef,
        RE::TESObjectREFR* a_activatorRef,
        std::uint8_t a_arg3,
        RE::TESBoundObject* a_object,
        std::int32_t a_targetCount);

    static inline FnActivate _originalActivateCONT = nullptr;
    static inline FnActivate _originalActivateDOOR = nullptr;

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

    bool Hooked_ActivateDOOR(
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
        return _originalActivateDOOR(a_this, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount);
    }

    void Install()
    {
        g_lockpick = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000A);

        SKSE::log::info("Hooking TESObjectCONT (Containers)...");
        REL::Relocation<std::uintptr_t> contVtbl{ RE::VTABLE_TESObjectCONT[0] };
        _originalActivateCONT = reinterpret_cast<FnActivate>(contVtbl.write_vfunc(0x37, Hooked_ActivateCONT));
        _originalCalculateDoFavorCONT = reinterpret_cast<FnCalculateDoFavor>(contVtbl.write_vfunc(0x4D, Hooked_CalculateDoFavorCONT));

        SKSE::log::info("Hooking TESObjectDOOR (Doors)...");
        REL::Relocation<std::uintptr_t> doorVtbl{ RE::VTABLE_TESObjectDOOR[0] };
        _originalActivateDOOR = reinterpret_cast<FnActivate>(doorVtbl.write_vfunc(0x37, Hooked_ActivateDOOR));
        _originalCalculateDoFavorDOOR = reinterpret_cast<FnCalculateDoFavor>(doorVtbl.write_vfunc(0x4D, Hooked_CalculateDoFavorDOOR));

        SKSE::log::info("FollowerLockFix hooks installed successfully!");
    }
}

void SetupLog()
{
    auto path = SKSE::log::log_directory();
    if (!path) {
        return;
    }

    *path /= "FollowerLockFix.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));

    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%T.%e] [%l] %v");
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    SetupLog();
    SKSE::Init(a_skse);

    SKSE::log::info("FollowerLockFix loading...");

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            FollowerLockFix::Install();
        }
    });

    return true;
}
