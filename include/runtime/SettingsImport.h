#pragma once

#include "runtime/ShipBindings.h"

#include <string>
#include <vector>

namespace AbsoluteControl::Runtime
{
    struct ImportedSettings
    {
        // Absolute Control's own settings, complete: every key the file has.
        std::vector<SettingEntry> entries;
        // What could not be brought across, in words a player can act on.
        std::vector<std::string> dropped;
    };

    // Turns AbsoluteHOTAS's settings into Absolute Control's. It runs once, the first time
    // Absolute Control starts with HOTAS files present and no file of its own. After that
    // the HOTAS files are never read again.
    [[nodiscard]] ImportedSettings ImportHotasSettings(const IniValues& hotas);
}
