#pragma once

#include "runtime/LegacyHotasToken.h"

#include <array>
#include <string_view>

namespace LegacyHotasFixtures
{
    using namespace AbsoluteControl::Runtime::LegacyHotas;
    inline constexpr std::string_view kGuid = "{01234567-89AB-CDEF-0123-456789ABCDEF}";

    // Source-derived fixtures, not recorded hardware observations. Source
    // locations and dialect differences are documented in checkpoint 03.
    struct Case
    {
        std::string_view suffix;
        Dialect dialect;
        Status status;
        Kind kind{};
        std::uint32_t index{};
        std::uint32_t direction{};
    };

    inline constexpr std::array kCases{
        Case{ "@button:1", Dialect::Unspecified, Status::Ok, Kind::Button, 0 },
        Case{ "@button:128", Dialect::Unspecified, Status::Ok, Kind::Button, 127 },
        Case{ "@BUTTON:001", Dialect::ZeroPower, Status::Ok, Kind::Button, 0 },
        Case{ "@BUTTON:001", Dialect::HotasCapture, Status::Invalid },
        Case{ "@pov:0:up", Dialect::Unspecified, Status::Ok, Kind::PovDirection, 0, 0 },
        Case{ "@pov:1:right", Dialect::Unspecified, Status::AmbiguousDialect },
        Case{ "@pov:1:right", Dialect::HotasCapture, Status::Ok, Kind::PovDirection, 1, 1 },
        Case{ "@pov:1:right", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 0, 1 },
        Case{ "@pov:3:left", Dialect::HotasCapture, Status::Ok, Kind::PovDirection, 3, 3 },
        Case{ "@pov:3:left", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 2, 3 },
        Case{ "@pov:4:N", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 3, 0 },
        Case{ "@pov:4:up", Dialect::HotasCapture, Status::Invalid },
        Case{ "@pov:2:EAST", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 1, 1 },
        Case{ "@pov:2:2", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 1, 2 },
        Case{ "@pov:3:w", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 2, 3 },
        Case{ "@pov:0", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 0, 0 },
        Case{ "@pov:15", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 3, 3 },
        Case{ "@pov:129", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 0, 0 },
        Case{ "@pov:144", Dialect::ZeroPower, Status::Ok, Kind::PovDirection, 3, 3 },
        Case{ "@pov:129", Dialect::HotasCapture, Status::Invalid },
        Case{ "@pov:16", Dialect::ZeroPower, Status::Invalid },
        Case{ "@pov:128", Dialect::ZeroPower, Status::Invalid },
        Case{ "@pov:145", Dialect::ZeroPower, Status::Invalid },
        Case{ "@pov:4:SW", Dialect::ZeroPower, Status::Invalid },
        Case{ "@pov:5:up", Dialect::ZeroPower, Status::Invalid },
        Case{ "@pov:0:up:extra", Dialect::Unspecified, Status::Invalid },
        Case{ "@axis:0x30", Dialect::HotasCapture, Status::Ok, Kind::Axis, 0 },
        Case{ "@axis:0x37", Dialect::Unspecified, Status::Ok, Kind::Axis, 7 },
        Case{ "@axis:0x30", Dialect::ZeroPower, Status::Invalid },
        Case{ "@axis:48", Dialect::HotasCapture, Status::Invalid },
        Case{ "@axis:0X30", Dialect::HotasCapture, Status::Invalid },
        Case{ "@axis:0x2F", Dialect::HotasCapture, Status::Invalid },
        Case{ "@axis:0x38", Dialect::HotasCapture, Status::Invalid },
        Case{ "@button:0", Dialect::Unspecified, Status::Invalid },
        Case{ "@button:129", Dialect::Unspecified, Status::Invalid },
        Case{ "@button:+1", Dialect::Unspecified, Status::Invalid },
        Case{ "@button:-1", Dialect::Unspecified, Status::Invalid },
        Case{ "@button:4294967296", Dialect::Unspecified, Status::Invalid },
        Case{ "@button:1junk", Dialect::Unspecified, Status::Invalid },
        Case{ "@button:1 ", Dialect::Unspecified, Status::Invalid },
        Case{ "@button: 1", Dialect::Unspecified, Status::Invalid },
        Case{ "@", Dialect::Unspecified, Status::Invalid }
    };
}
