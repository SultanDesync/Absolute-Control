#include "AbsoluteControlSubscriberKit.h"

#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <string_view>

using namespace AbsoluteControlPanelApi;
namespace Kit = AbsoluteControlSubscriberKit;

namespace
{
    std::uint32_t g_registeredModules{};
    std::uint32_t g_registeredPages{};
    std::uint32_t g_unregisteredModules{};
    std::array<Result, 4> g_pageResults{};
    Result g_moduleResult{ Result::Ok };
    Result g_unregisterResult{ Result::Ok };

    struct Configuration
    {
        bool enabled{};
        double sensitivity{ 1.0 };
    };

    Configuration g_configuration{};
    std::uint32_t g_saveCount{};
    bool g_applySucceeds{ true };
    bool g_saveSucceeds{ true };

    Configuration GetConfiguration() { return g_configuration; }
    bool ApplyConfiguration(const Configuration& value)
    {
        if (!g_applySucceeds) return false;
        g_configuration = value;
        return true;
    }
    bool SaveConfiguration()
    {
        ++g_saveCount;
        return g_saveSucceeds;
    }

    Result __cdecl RegisterPage(const PageDescriptorV1*) noexcept
    {
        assert(g_registeredPages < g_pageResults.size());
        return g_pageResults[g_registeredPages++];
    }

    Result __cdecl RegisterModule(const ModuleDescriptorV1*) noexcept
    {
        ++g_registeredModules;
        return g_moduleResult;
    }

    Result __cdecl UnregisterModule(const char*) noexcept
    {
        ++g_unregisteredModules;
        return g_unregisterResult;
    }

    Result __cdecl Refresh(const char*, const char*) noexcept
    {
        return Result::Ok;
    }

    std::uint8_t __cdecl True() noexcept { return 1; }

    ApiV1 Host()
    {
        ApiV1 result;
        result.moduleId = kModuleId.data();
        result.registerPage = &RegisterPage;
        result.unregisterModule = &UnregisterModule;
        result.requestRefresh = &Refresh;
        result.registerModule = &RegisterModule;
        result.isOpen = &True;
        result.isInputCaptureActive = &True;
        result.capabilities = kCapabilityStructuredLayout;
        return result;
    }

    void ResetRegistrationHost()
    {
        g_registeredModules = 0;
        g_registeredPages = 0;
        g_unregisteredModules = 0;
        g_pageResults.fill(Result::Ok);
        g_moduleResult = Result::Ok;
        g_unregisterResult = Result::Ok;
    }
}

int main()
{
    bool boolean{};
    assert(Kit::ReadBoolean(Kit::Boolean(true), boolean) && boolean);
    auto invalidBoolean = Kit::Boolean(false);
    invalidBoolean.booleanValue = 2;
    assert(!Kit::ReadBoolean(invalidBoolean, boolean));

    std::int64_t integer{};
    assert(Kit::ReadInteger(Kit::Integer(30), 10, 50, 10, integer));
    assert(integer == 30);
    assert(!Kit::ReadInteger(Kit::Integer(31), 10, 50, 10, integer));

    double number{};
    assert(Kit::ReadFloat(Kit::Float(0.5), 0.0, 1.0, number));
    assert(!Kit::ReadFloat(Kit::Float(
        std::numeric_limits<double>::quiet_NaN()), 0.0, 1.0, number));

    ValueV1 text;
    assert(Kit::String("binding-token", text));
    std::string_view decoded;
    assert(Kit::ReadString(text, decoded) && decoded == "binding-token");
    assert(!Kit::String(std::string_view(
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"), text));

    auto host = Host();
    assert(Kit::ValidHostApi(&host));
    assert(Kit::Supports(&host, kCapabilityStructuredLayout));
    assert(Kit::IsOpen(&host));
    assert(Kit::IsInputCaptureActive(&host));

    ModuleDescriptorV1 module;
    assert(Kit::CopyText(module.moduleId, "example.module"));
    assert(Kit::CopyText(module.displayName, "Example Module"));
    std::array<PageDescriptorV1, 2> pages{};
    assert(Kit::CopyText(pages[0].moduleId, "example.module"));
    assert(Kit::CopyText(pages[0].pageId, "general"));
    assert(Kit::CopyText(pages[1].moduleId, "example.module"));
    assert(Kit::CopyText(pages[1].pageId, "bindings"));

    ResetRegistrationHost();
    assert(Kit::RegisterModulePages(&host, module, pages) == Result::Ok);
    assert(g_registeredModules == 1);
    assert(g_registeredPages == 2);
    assert(g_unregisteredModules == 0);

    // A duplicate module may belong to another provider instance. Fail closed
    // without appending pages or trying to unregister it.
    ResetRegistrationHost();
    g_moduleResult = Result::Duplicate;
    const auto duplicateModule =
        Kit::RegisterModulePagesDetailed(&host, module, pages);
    assert(duplicateModule.result == Result::Duplicate);
    assert(!duplicateModule.rollbackAttempted);
    assert(g_registeredModules == 1);
    assert(g_registeredPages == 0);
    assert(g_unregisteredModules == 0);

    // A duplicate page is not idempotent success. Since this invocation did
    // create the module, it attempts to remove the partial registration.
    ResetRegistrationHost();
    g_pageResults[0] = Result::Duplicate;
    const auto duplicatePage =
        Kit::RegisterModulePagesDetailed(&host, module, pages);
    assert(duplicatePage.result == Result::Duplicate);
    assert(duplicatePage.RollbackSucceeded());
    assert(!duplicatePage.PartialRegistrationMayRemain());
    assert(g_registeredPages == 1);
    assert(g_unregisteredModules == 1);

    // Descriptor relationships and page-ID uniqueness are checked before any
    // host mutation.
    auto mismatchedPages = pages;
    assert(Kit::CopyText(mismatchedPages[1].moduleId, "foreign.module"));
    ResetRegistrationHost();
    assert(Kit::RegisterModulePages(&host, module, mismatchedPages) ==
        Result::InvalidArgument);
    assert(g_registeredModules == 0 && g_registeredPages == 0 &&
        g_unregisteredModules == 0);

    auto duplicateIds = pages;
    assert(Kit::CopyText(duplicateIds[1].pageId, "general"));
    ResetRegistrationHost();
    assert(Kit::RegisterModulePages(&host, module, duplicateIds) ==
        Result::InvalidArgument);
    assert(g_registeredModules == 0 && g_registeredPages == 0);

    auto emptyId = pages;
    emptyId[1].pageId[0] = '\0';
    ResetRegistrationHost();
    assert(Kit::RegisterModulePages(&host, module, emptyId) ==
        Result::InvalidArgument);
    assert(g_registeredModules == 0 && g_registeredPages == 0);

    // Failure after a successfully registered first page is surfaced together
    // with the successful rollback outcome.
    ResetRegistrationHost();
    g_pageResults[1] = Result::Rejected;
    const auto pageFailure =
        Kit::RegisterModulePagesDetailed(&host, module, pages);
    assert(pageFailure.result == Result::Rejected);
    assert(pageFailure.RollbackSucceeded());
    assert(g_registeredPages == 2);
    assert(g_unregisteredModules == 1);

    // A rejected rollback is observable; callers must assume that the module
    // and the first page may still be registered.
    ResetRegistrationHost();
    g_pageResults[1] = Result::Rejected;
    g_unregisterResult = Result::Rejected;
    const auto rejectedRollback =
        Kit::RegisterModulePagesDetailed(&host, module, pages);
    assert(rejectedRollback.result == Result::Rejected);
    assert(rejectedRollback.rollbackAttempted);
    assert(rejectedRollback.rollbackResult == Result::Rejected);
    assert(!rejectedRollback.RollbackSucceeded());
    assert(rejectedRollback.PartialRegistrationMayRemain());
    assert(g_registeredPages == 2);
    assert(g_unregisteredModules == 1);

    // The compatibility wrapper still returns the registration failure.
    ResetRegistrationHost();
    g_pageResults[1] = Result::WriteFailure;
    assert(Kit::RegisterModulePages(&host, module, pages) ==
        Result::WriteFailure);

    Kit::DraftTransaction<Configuration> transaction({
        &GetConfiguration, &ApplyConfiguration, &SaveConfiguration });
    assert(transaction.Mutate([](Configuration& value) {
        value.enabled = true;
        return true;
    }) == Result::Ok);
    assert(transaction.IsEditing() && g_configuration.enabled);
    assert(transaction.Mutate([](Configuration& value) {
        value.sensitivity = 2.5;
        return true;
    }) == Result::Ok);
    transaction.Cancel();
    assert(!transaction.IsEditing());
    assert(!g_configuration.enabled && g_configuration.sensitivity == 1.0);

    assert(transaction.Mutate([](Configuration& value) {
        value.enabled = true;
        return true;
    }) == Result::Ok);
    assert(transaction.Commit() == Result::Ok);
    assert(g_saveCount == 1 && !transaction.IsEditing());

    g_applySucceeds = false;
    assert(transaction.Mutate([](Configuration& value) {
        value.enabled = false;
        return true;
    }) == Result::WriteFailure);
    assert(!transaction.IsEditing() && g_configuration.enabled);
}
