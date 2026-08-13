#include "SlopSubscriber.h"
#include "Configuration.h"
#include "RuntimePaths.h"

#include <cassert>
#include <cstring>

namespace Configuration
{
    Config Get() { return ConfigPolicy::Defaults(); }
    bool Apply(const Config&) { return true; }
    bool Save() { return true; }
}

namespace RuntimePaths
{
    void AppendLog(const char*, const std::string&) {}
}

namespace
{
    using namespace SlopApi;
    int g_registerCalls{};
    Result g_registerResult{ Result::Ok };

    Result __cdecl RegisterPage(const PageDescriptorV1*) noexcept
    {
        ++g_registerCalls;
        return g_registerResult;
    }
    Result __cdecl Unregister(const char*) noexcept { return Result::Ok; }
    Result __cdecl Refresh(const char*, const char*) noexcept { return Result::Ok; }

    ApiV1 g_api;
    const ApiV1* __cdecl NullQuery(std::uint32_t) noexcept { return nullptr; }
    const ApiV1* __cdecl GoodQuery(std::uint32_t version) noexcept
    {
        return version == kAbiVersion ? &g_api : nullptr;
    }

    PageDescriptorV1 Page()
    {
        PageDescriptorV1 page;
        strcpy_s(page.moduleId, "acceptance.subscriber");
        strcpy_s(page.pageId, "general");
        strcpy_s(page.displayName, "General");
        return page;
    }
}

int main()
{
    using namespace SlopApi;
    const auto page = Page();

    assert(SlopSubscriber::RegisterWith(nullptr, page) == Result::NotFound);
    assert(SlopSubscriber::RegisterWith(&NullQuery, page) == Result::NotReady);

    g_api = {};
    g_api.moduleId = "FOREIGN";
    g_api.displayName = "Foreign";
    g_api.version = "1";
    g_api.registerPage = &RegisterPage;
    g_api.unregisterModule = &Unregister;
    g_api.requestRefresh = &Refresh;
    assert(SlopSubscriber::RegisterWith(&GoodQuery, page) == Result::Rejected);

    g_api.moduleId = "SLOP";
    g_registerCalls = 0;
    g_registerResult = Result::Duplicate;
    assert(SlopSubscriber::RegisterWith(&GoodQuery, page) == Result::Duplicate);
    assert(g_registerCalls == 1);
    return 0;
}
