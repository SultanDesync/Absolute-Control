#include "MenuApiHost.h"
#include "SlopAPI.h"

#include <cstring>

#define CHECK(expression)            \
    do {                             \
        if (!(expression)) return 1; \
    } while (false)

namespace
{
    SlopApi::Result __cdecl ReadValue(
        void*, const char*, SlopApi::ValueV1*) noexcept
    {
        return SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl WriteDraft(
        void*, const char*, const SlopApi::ValueV1*) noexcept
    {
        return SlopApi::Result::Ok;
    }
}

int main()
{
    using namespace SlopApi;

    CHECK(SLOP_QueryApi(kAbiVersion + 1) == nullptr);
    const auto* api = SLOP_QueryApi(kAbiVersion);
    CHECK(api != nullptr);
    CHECK(api->abiVersion == kAbiVersion);
    CHECK(api->structSize >= sizeof(ApiV1));

    CHECK(api->registerPage(nullptr) == Result::InvalidArgument);

    ControlDescriptorV1 control;
    control.kind = ControlKind::Toggle;
    strcpy_s(control.controlId, "enabled");
    strcpy_s(control.label, "Enabled");

    PageDescriptorV1 page;
    strcpy_s(page.moduleId, "test.module");
    strcpy_s(page.pageId, "general");
    strcpy_s(page.displayName, "General");
    page.controlCount = 1;
    page.controls = &control;
    page.readValue = &ReadValue;
    page.writeDraft = &WriteDraft;

    CHECK(api->registerPage(&page) == Result::Ok);
    CHECK(api->registerPage(&page) == Result::Duplicate);

    // Registration copies all presentation descriptors. A provider may release or
    // overwrite the input records immediately after registerPage returns.
    strcpy_s(control.label, "MUTATED");
    const auto copied = AbsoluteControlPanelResearch::MenuApiHost::FindPage(
        "test.module", "general");
    CHECK(copied.has_value());
    CHECK(copied->controls.size() == 1);
    CHECK(copied->controls.front().label == "Enabled");

    CHECK(api->requestRefresh("test.module", "general") == Result::Ok);
    CHECK(api->requestRefresh("test.module", "missing") == Result::NotFound);
    CHECK(api->unregisterModule("test.module") == Result::Ok);
    CHECK(api->unregisterModule("test.module") == Result::NotFound);

    page.moduleId[0] = '\0';
    CHECK(api->registerPage(&page) == Result::InvalidArgument);
    return 0;
}
