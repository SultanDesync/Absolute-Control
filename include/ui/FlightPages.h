#pragma once

namespace AbsoluteControlPanelResearch::Ui::FlightPages
{
    // Adds the Flight Axes and Ship Buttons pages to the menu. Call once the settings are
    // loaded. What they change is saved to Absolute Control's own file and takes effect on
    // Apply, without a restart.
    void Register() noexcept;
}
