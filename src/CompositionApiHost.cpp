#include "AbsoluteControlCompositionExperimentalAPI.h"

#include "CompositionRegistry.h"
#include "MenuApiHost.h"

namespace
{
    namespace Api = AbsoluteControlCompositionExperimental;
    using StableResult = AbsoluteControlPanelApi::Result;

    [[nodiscard]] Api::Result __cdecl RegisterComposition(
        const Api::PageCompositionDescriptorV1* descriptor) noexcept
    {
        using namespace AbsoluteControlPanelResearch;
        if (MenuApiHost::Lifecycle() ==
            MenuApiHost::HostLifecycle::Initializing) {
            return StableResult::NotReady;
        }
        if (MenuApiHost::Lifecycle() ==
            MenuApiHost::HostLifecycle::Rejected) {
            return StableResult::Rejected;
        }
        if (!descriptor) return StableResult::InvalidArgument;
        const auto page = MenuApiHost::FindPage(
            descriptor->moduleId, descriptor->pageId);
        if (!page) return StableResult::NotFound;
        return Composition::HostRegistry().Register(
            *descriptor, *page,
            Api::kC2Capabilities | Api::kCapabilityDirectLiveManipulation);
    }

    [[nodiscard]] Api::Result __cdecl UnregisterComposition(
        const char* moduleId) noexcept
    {
        using namespace AbsoluteControlPanelResearch;
        if (MenuApiHost::Lifecycle() != MenuApiHost::HostLifecycle::Ready) {
            return StableResult::Rejected;
        }
        return Composition::HostRegistry().UnregisterModule(moduleId);
    }

    [[nodiscard]] Api::Result __cdecl RefreshComposition(
        const char* moduleId, const char* pageId) noexcept
    {
        return AbsoluteControlPanelResearch::MenuApiHost::RequestRefresh(
            moduleId, pageId);
    }

    const Api::ApiV1 g_compositionApi{
        sizeof(Api::ApiV1),
        Api::kAbiVersion,
        AbsoluteControlPanelApi::kModuleId.data(),
        "Absolute Control semantic composition C2",
        Api::kC2Capabilities | Api::kCapabilityDirectLiveManipulation,
        &RegisterComposition,
        &UnregisterComposition,
        &RefreshComposition
    };
}

extern "C" ABSOLUTE_CONTROL_PANEL_API
const AbsoluteControlCompositionExperimental::ApiV1*
AbsoluteControlPanel_QueryCompositionApi(
    std::uint32_t requestedAbiVersion) noexcept
{
    if (requestedAbiVersion !=
        AbsoluteControlCompositionExperimental::kAbiVersion) return nullptr;
    return &g_compositionApi;
}
