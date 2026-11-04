#pragma once

namespace AbsoluteControlPanelResearch::Ui::WalkGaugeMenu
{
    // Adds the speed gauge overlay to the game's menu list.
    void Register() noexcept;

    // Safe from any thread. The movie reads these itself each frame, so nothing calls into it
    // from a thread it isn't running on.
    void Publish(double speed, double alpha) noexcept;

    // Opens the overlay if it isn't open. It then stays open at zero opacity, like the game's own
    // HUD messages menu, so a speed change doesn't have to load a movie each time.
    void EnsureOpen() noexcept;
}
