#include "AbsoluteHeadTrackingMenu.generated.h"

#include <cassert>

using namespace AbsoluteControlPanelApi;
namespace Generated = AbsoluteControlPanelGenerated::module_absolute_head_tracking;

namespace
{
    Result __cdecl Read(void*, const char*, ValueV1*) noexcept { return Result::Ok; }
    Result __cdecl Write(void*, const char*, const ValueV1*) noexcept { return Result::Ok; }
    Result __cdecl Action(void*, const char*) noexcept { return Result::Ok; }
    Result __cdecl Apply(void*) noexcept { return Result::Ok; }
    void __cdecl Cancel(void*) noexcept {}
}

int main()
{
    static_assert(Generated::kModule.structSize == sizeof(ModuleDescriptorV1));
    static_assert(Generated::kAxesControls.size() == 12);
    static_assert(Generated::ParseControlId("pitch.inverted") == Generated::ControlId::id_pitch_inverted);
    const Generated::ProviderCallbacks callbacks{ nullptr, Read, Write, Action, Apply, Cancel };
    const auto pages = Generated::MakePages(callbacks);
    assert(pages.size() == 3);
    assert(pages[2].readValue == Read);
    assert(pages[2].controls == Generated::kBindingsControls.data());
}
