#pragma once

// Source-compatible adapter for subscribers built against the original SLOP
// name. AbsoluteControlPanelAPI.h is the single authority for every ABI value,
// descriptor, and callback signature. Only the legacy ApiV1 prefix and export
// name remain distinct.

#include "AbsoluteControlPanelAPI.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace SlopApi
{
    inline constexpr auto kAbiVersion = AbsoluteControlPanelApi::kAbiVersion;
    inline constexpr auto kIdentifierCapacity =
        AbsoluteControlPanelApi::kIdentifierCapacity;
    inline constexpr auto kLabelCapacity = AbsoluteControlPanelApi::kLabelCapacity;
    inline constexpr auto kDescriptionCapacity =
        AbsoluteControlPanelApi::kDescriptionCapacity;
    inline constexpr auto kStringValueCapacity =
        AbsoluteControlPanelApi::kStringValueCapacity;

    using Result = AbsoluteControlPanelApi::Result;
    using ControlKind = AbsoluteControlPanelApi::ControlKind;
    using ValueKind = AbsoluteControlPanelApi::ValueKind;
    using ControlFlags = AbsoluteControlPanelApi::ControlFlags;

    inline constexpr auto kControlNone = AbsoluteControlPanelApi::kControlNone;
    inline constexpr auto kControlReadOnly = AbsoluteControlPanelApi::kControlReadOnly;
    inline constexpr auto kControlRequiresRestart =
        AbsoluteControlPanelApi::kControlRequiresRestart;
    inline constexpr auto kControlAdvanced = AbsoluteControlPanelApi::kControlAdvanced;
    inline constexpr auto kBindingKeyboard = AbsoluteControlPanelApi::kBindingKeyboard;
    inline constexpr auto kBindingMouse = AbsoluteControlPanelApi::kBindingMouse;
    inline constexpr auto kBindingController = AbsoluteControlPanelApi::kBindingController;
    inline constexpr auto kBindingModifiers = AbsoluteControlPanelApi::kBindingModifiers;
    inline constexpr auto kBindingClearable = AbsoluteControlPanelApi::kBindingClearable;

    using ValueV1 = AbsoluteControlPanelApi::ValueV1;
    using ControlDescriptorV1 = AbsoluteControlPanelApi::ControlDescriptorV1;
    using ReadValueCallback = AbsoluteControlPanelApi::ReadValueCallback;
    using WriteDraftCallback = AbsoluteControlPanelApi::WriteDraftCallback;
    using InvokeActionCallback = AbsoluteControlPanelApi::InvokeActionCallback;
    using ApplyCallback = AbsoluteControlPanelApi::ApplyCallback;
    using CancelCallback = AbsoluteControlPanelApi::CancelCallback;
    using ModuleDescriptorV1 = AbsoluteControlPanelApi::ModuleDescriptorV1;
    using PageDescriptorV1 = AbsoluteControlPanelApi::PageDescriptorV1;

    // This is the exact prefix shipped by the research API. New product fields
    // remain outside the legacy struct so an old provider never reads past it.
    struct ApiV1
    {
        std::uint32_t structSize{ sizeof(ApiV1) };
        std::uint32_t abiVersion{ kAbiVersion };
        const char* moduleId{};
        const char* displayName{};
        const char* version{};
        Result(__cdecl* registerPage)(const PageDescriptorV1*) noexcept{};
        Result(__cdecl* unregisterModule)(const char*) noexcept{};
        Result(__cdecl* requestRefresh)(const char*, const char*) noexcept{};
        Result(__cdecl* registerModule)(const ModuleDescriptorV1*) noexcept{};
    };

    static_assert(std::is_same_v<ValueV1, AbsoluteControlPanelApi::ValueV1>);
    static_assert(std::is_same_v<ControlDescriptorV1,
        AbsoluteControlPanelApi::ControlDescriptorV1>);
    static_assert(std::is_same_v<PageDescriptorV1,
        AbsoluteControlPanelApi::PageDescriptorV1>);
    static_assert(std::is_same_v<ReadValueCallback,
        AbsoluteControlPanelApi::ReadValueCallback>);
    static_assert(std::is_same_v<WriteDraftCallback,
        AbsoluteControlPanelApi::WriteDraftCallback>);
    static_assert(std::is_same_v<InvokeActionCallback,
        AbsoluteControlPanelApi::InvokeActionCallback>);
    static_assert(std::is_same_v<ApplyCallback, AbsoluteControlPanelApi::ApplyCallback>);
    static_assert(std::is_same_v<CancelCallback, AbsoluteControlPanelApi::CancelCallback>);
    static_assert(std::is_standard_layout_v<ApiV1>);

    // The legacy table is a strict prefix-compatible view of the product table.
    static_assert(offsetof(ApiV1, structSize) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, structSize));
    static_assert(offsetof(ApiV1, abiVersion) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, abiVersion));
    static_assert(offsetof(ApiV1, moduleId) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, moduleId));
    static_assert(offsetof(ApiV1, displayName) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, displayName));
    static_assert(offsetof(ApiV1, version) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, version));
    static_assert(offsetof(ApiV1, registerPage) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, registerPage));
    static_assert(offsetof(ApiV1, unregisterModule) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, unregisterModule));
    static_assert(offsetof(ApiV1, requestRefresh) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, requestRefresh));
    static_assert(offsetof(ApiV1, registerModule) ==
        offsetof(AbsoluteControlPanelApi::ApiV1, registerModule));
    static_assert(sizeof(ApiV1) == offsetof(AbsoluteControlPanelApi::ApiV1, isOpen));
}

#if defined(SLOP_EXPORTS)
#define SLOP_API __declspec(dllexport)
#else
#define SLOP_API __declspec(dllimport)
#endif

extern "C" SLOP_API const SlopApi::ApiV1*
SLOP_QueryApi(std::uint32_t requestedAbiVersion) noexcept;
