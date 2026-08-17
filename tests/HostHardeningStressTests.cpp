#include "AbsoluteControlPanelAPI.h"
#include "MenuApiHost.h"
#include "MenuSession.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#define CHECK(expression)            \
    do {                             \
        if (!(expression)) return 1; \
    } while (false)

namespace
{
    using AbsoluteControlPanelApi::ApiV1;
    using AbsoluteControlPanelApi::ControlDescriptorV1;
    using AbsoluteControlPanelApi::ControlKind;
    using AbsoluteControlPanelApi::ModuleDescriptorV1;
    using AbsoluteControlPanelApi::PageDescriptorV1;
    using AbsoluteControlPanelApi::Result;
    using AbsoluteControlPanelApi::ValueKind;
    using AbsoluteControlPanelApi::ValueV1;

    struct Provider
    {
        std::atomic_uint64_t reads{};
    };

    struct ReentrantProvider
    {
        const ApiV1* api{};
        std::atomic_uint64_t reads{};
        char moduleId[AbsoluteControlPanelApi::kIdentifierCapacity]{};
        char pageId[AbsoluteControlPanelApi::kIdentifierCapacity]{};
    };

    Result __cdecl ReadBoolean(
        void* a_context, const char*, ValueV1* a_value) noexcept
    {
        static_cast<Provider*>(a_context)->reads.fetch_add(
            1, std::memory_order_relaxed);
        a_value->structSize = sizeof(*a_value);
        a_value->kind = ValueKind::Boolean;
        a_value->booleanValue = 1;
        return Result::Ok;
    }

    Result __cdecl ReadAndRefresh(
        void* a_context, const char*, ValueV1* a_value) noexcept
    {
        auto& provider = *static_cast<ReentrantProvider*>(a_context);
        provider.reads.fetch_add(1, std::memory_order_relaxed);
        const auto refresh = provider.api->requestRefresh(
            provider.moduleId, provider.pageId);
        if (refresh != Result::Ok) {
            return refresh;
        }
        a_value->structSize = sizeof(*a_value);
        a_value->kind = ValueKind::Boolean;
        a_value->booleanValue = 1;
        return Result::Ok;
    }

    ControlDescriptorV1 MakeReadOnlyToggle(std::size_t a_index)
    {
        ControlDescriptorV1 control;
        control.kind = ControlKind::Toggle;
        control.flags = AbsoluteControlPanelApi::kControlReadOnly;
        std::snprintf(
            control.controlId, sizeof(control.controlId), "value.%02zu", a_index);
        std::snprintf(
            control.label, sizeof(control.label), "Value %02zu", a_index);
        return control;
    }
}

int main()
{
    using namespace AbsoluteControlPanelResearch;
    using namespace std::chrono_literals;

    const auto* api = AbsoluteControlPanel_QueryApi(
        AbsoluteControlPanelApi::kAbiVersion);
    CHECK(api != nullptr);
    MenuApiHost::MarkRuntimeReady();

    constexpr std::size_t kModules = 64;
    constexpr std::size_t kPagesPerModule = 3;
    constexpr std::size_t kControlsPerPage = 8;
    std::array<ControlDescriptorV1, kControlsPerPage> controls;
    for (std::size_t index{}; index < controls.size(); ++index) {
        controls[index] = MakeReadOnlyToggle(index);
    }
    std::array<Provider, kModules> providers;
    std::array<std::atomic_bool, kModules> registrationSucceeded{};
    std::atomic_bool registrationFailed{};
    std::vector<std::thread> registrars;
    registrars.reserve(kModules);
    for (std::size_t moduleIndex{}; moduleIndex < kModules; ++moduleIndex) {
        registrars.emplace_back([&, moduleIndex] {
            ModuleDescriptorV1 module;
            std::snprintf(module.moduleId, sizeof(module.moduleId),
                "stress.module.%03zu", moduleIndex);
            std::snprintf(module.displayName, sizeof(module.displayName),
                "Stress Module %03zu", moduleIndex);
            if (api->registerModule(&module) != Result::Ok) {
                registrationFailed.store(true, std::memory_order_release);
                return;
            }
            for (std::size_t pageIndex{}; pageIndex < kPagesPerModule;
                 ++pageIndex) {
                PageDescriptorV1 page;
                strcpy_s(page.moduleId, module.moduleId);
                std::snprintf(page.pageId, sizeof(page.pageId),
                    "page.%zu", pageIndex);
                std::snprintf(page.displayName, sizeof(page.displayName),
                    "Page %zu", pageIndex);
                page.controlCount = static_cast<std::uint32_t>(controls.size());
                page.controls = controls.data();
                page.context = &providers[moduleIndex];
                page.readValue = &ReadBoolean;
                if (api->registerPage(&page) != Result::Ok) {
                    registrationFailed.store(true, std::memory_order_release);
                    return;
                }
            }
            registrationSucceeded[moduleIndex].store(
                true, std::memory_order_release);
        });
    }
    for (auto& registrar : registrars) registrar.join();
    CHECK(!registrationFailed.load(std::memory_order_acquire));
    for (const auto& succeeded : registrationSucceeded) {
        CHECK(succeeded.load(std::memory_order_acquire));
    }

    // Provider callbacks may reenter the public host API. Snapshotting must not
    // hold the registry/provider mutex while executing subscriber code.
    ModuleDescriptorV1 reentrantModule;
    strcpy_s(reentrantModule.moduleId, "stress.reentrant");
    strcpy_s(reentrantModule.displayName, "Reentrant Provider");
    CHECK(api->registerModule(&reentrantModule) == Result::Ok);
    ReentrantProvider reentrant{ .api = api };
    strcpy_s(reentrant.moduleId, reentrantModule.moduleId);
    strcpy_s(reentrant.pageId, "general");
    auto reentrantControl = MakeReadOnlyToggle(0);
    PageDescriptorV1 reentrantPage;
    strcpy_s(reentrantPage.moduleId, reentrant.moduleId);
    strcpy_s(reentrantPage.pageId, reentrant.pageId);
    strcpy_s(reentrantPage.displayName, "General");
    reentrantPage.controlCount = 1;
    reentrantPage.controls = &reentrantControl;
    reentrantPage.context = &reentrant;
    reentrantPage.readValue = &ReadAndRefresh;
    CHECK(api->registerPage(&reentrantPage) == Result::Ok);
    MenuSession::Session reentrantSession;
    MenuSession::Command selectReentrant;
    selectReentrant.kind = MenuSession::CommandKind::SelectPage;
    selectReentrant.moduleId = reentrant.moduleId;
    selectReentrant.pageId = reentrant.pageId;
    const auto reentrantModel = reentrantSession.Dispatch(selectReentrant);
    CHECK(reentrantModel.error.empty());
    CHECK(reentrant.reads.load(std::memory_order_acquire) == 1);
    CHECK(api->unregisterModule(reentrant.moduleId) == Result::Ok);

    std::atomic_bool start{};
    std::atomic_bool stop{};
    std::atomic_bool stressFailed{};
    std::vector<std::thread> workers;

    // Independent sessions mirror multiple readers/diagnostic clients without
    // violating the real menu session's deliberate single-UI-thread contract.
    for (std::size_t workerIndex{}; workerIndex < 4; ++workerIndex) {
        workers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            MenuSession::Session session;
            while (!stop.load(std::memory_order_acquire)) {
                const auto model = session.Snapshot();
                std::size_t controlsInSnapshot{};
                for (const auto& page : model.pages) {
                    controlsInSnapshot += page.controls.size();
                }
                if (!model.pages.empty() &&
                    controlsInSnapshot > kControlsPerPage) {
                    stressFailed.store(true, std::memory_order_release);
                    return;
                }
            }
        });
    }
    for (std::size_t workerIndex{}; workerIndex < 4; ++workerIndex) {
        workers.emplace_back([&, workerIndex] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::size_t iteration{}; iteration < 1000; ++iteration) {
                const auto moduleIndex =
                    (iteration * 7 + workerIndex) % kModules;
                char moduleId[AbsoluteControlPanelApi::kIdentifierCapacity]{};
                std::snprintf(moduleId, sizeof(moduleId),
                    "stress.module.%03zu", moduleIndex);
                const auto result = api->requestRefresh(moduleId, "page.0");
                if (result != Result::Ok && result != Result::NotFound) {
                    stressFailed.store(true, std::memory_order_release);
                    return;
                }
            }
        });
    }
    start.store(true, std::memory_order_release);

    // Distinct modules unregister concurrently with catalog copies, provider
    // reads, and scoped refresh requests. Every provider context remains alive
    // until all workers join, matching the ABI's DLL-lifetime requirement.
    std::vector<std::thread> removers;
    removers.reserve(8);
    for (std::size_t removerIndex{}; removerIndex < 8; ++removerIndex) {
        removers.emplace_back([&, removerIndex] {
            for (std::size_t moduleIndex = removerIndex;
                 moduleIndex < kModules; moduleIndex += 8) {
                char moduleId[AbsoluteControlPanelApi::kIdentifierCapacity]{};
                std::snprintf(moduleId, sizeof(moduleId),
                    "stress.module.%03zu", moduleIndex);
                Result result = Result::Rejected;
                for (std::size_t attempt{}; attempt < 10000 &&
                     result == Result::Rejected; ++attempt) {
                    result = api->unregisterModule(moduleId);
                    if (result == Result::Rejected) std::this_thread::yield();
                }
                if (result != Result::Ok) {
                    stressFailed.store(true, std::memory_order_release);
                    return;
                }
            }
        });
    }
    for (auto& remover : removers) remover.join();
    stop.store(true, std::memory_order_release);
    for (auto& worker : workers) worker.join();

    CHECK(!stressFailed.load(std::memory_order_acquire));
    CHECK(MenuApiHost::SnapshotCatalog({}, {}).pages.empty());

    // Rejection is terminal and linearized against registry mutation.
    MenuApiHost::MarkRuntimeRejected();
    ModuleDescriptorV1 afterRejection;
    strcpy_s(afterRejection.moduleId, "stress.after-rejection");
    strcpy_s(afterRejection.displayName, "Rejected");
    CHECK(api->registerModule(&afterRejection) == Result::Rejected);
    return 0;
}
