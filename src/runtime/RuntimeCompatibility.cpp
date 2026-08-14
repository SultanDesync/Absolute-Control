#include "runtime/RuntimeCompatibility.h"

#include "EvidenceLog.h"

#include <RE/Starfield.h>

#include <array>
#include <format>
#include <string_view>

namespace AbsoluteControlPanelResearch::Runtime
{
    namespace
    {
        constexpr REL::Version kSupportedRuntime{ 1, 16, 244, 0 };

        struct VerifiedRelocation
        {
            std::string_view symbol;
            std::uint64_t id;
            std::uint64_t expectedOffset;
        };

        constexpr std::array kVerifiedRelocations{
            VerifiedRelocation{ "GameMenuBase::ctor", 130615, 0x025516B0 },
            VerifiedRelocation{ "GameMenuBase::Unk10", 93620, 0x01667080 },
            VerifiedRelocation{ "GameMenuBase::Unk11", 93621, 0x016670C0 },
            VerifiedRelocation{ "IMenu::dtor", 130617, 0x025518A0 },
            VerifiedRelocation{ "IMenu::ShouldHandleEvent", 91901, 0x02553390 },
            VerifiedRelocation{ "IMenu::OnThumbstickEvent", 130633, 0x02553670 },
            VerifiedRelocation{ "IMenu::OnButtonEvent", 130632, 0x025533D0 },
            VerifiedRelocation{ "IMenu::LoadMovie", 130618, 0x02551AB0 },
            VerifiedRelocation{ "IMenu::ProcessMessage", 130624, 0x02552070 },
            VerifiedRelocation{ "IMenu::Unk09", 42815, 0x00481670 },
            VerifiedRelocation{ "IMenu::Unk0E", 130622, 0x02551D70 },
            VerifiedRelocation{ "IMenu::Unk12", 42816, 0x00481680 },
            VerifiedRelocation{ "IMenu::Unk13", 39540, 0x003AE910 },
            VerifiedRelocation{ "IMenu::Unk19", 130634, 0x02553940 },
            VerifiedRelocation{ "UI::IsMenuOpen", 130475, 0x02544EC0 }
        };
    }

    std::uintptr_t ToImageRva(const void* a_address) noexcept
    {
        const auto caller = reinterpret_cast<std::uintptr_t>(a_address);
        const auto imageBase = REX::FModule::GetExecutingModule().GetBaseAddress();
        return caller >= imageBase ? caller - imageBase : 0;
    }

    bool ValidateMenuRelocations() noexcept
    {
        const auto runtime = REX::FModule::GetExecutingModule().GetFileVersion();
        if (runtime != kSupportedRuntime) {
            EvidenceLog::Event(
                "relocation_validation_failed",
                std::format("unsupported_runtime={} expected={}", runtime, kSupportedRuntime));
            return false;
        }

        const auto database = REL::IDDB::GetSingleton();
        for (const auto& mapping : kVerifiedRelocations) {
            const auto actualOffset = database->offset(mapping.id);
            if (actualOffset != mapping.expectedOffset) {
                EvidenceLog::Event(
                    "relocation_validation_failed",
                    std::format(
                        "symbol={} id={} actual=0x{:08X} expected=0x{:08X}",
                        mapping.symbol, mapping.id, actualOffset, mapping.expectedOffset));
                return false;
            }
        }

        EvidenceLog::Event(
            "relocation_validation_succeeded",
            std::format("runtime={} mapping_count={}", runtime, kVerifiedRelocations.size()));
        return true;
    }
}
