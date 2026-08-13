#include "MenuSession.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <ranges>

namespace AbsoluteControlPanelResearch::MenuSession
{
    namespace
    {
        constexpr std::size_t kMaximumPages = 32;
        constexpr std::size_t kMaximumControlsPerPage = 128;
        constexpr std::size_t kMaximumControls = 512;
        constexpr std::size_t kMaximumError = SlopApi::kDescriptionCapacity - 1;
        constexpr double kMaximumExactScaleformInteger = 9007199254740991.0;

        [[nodiscard]] bool ValidKind(SlopApi::ControlKind a_kind) noexcept
        {
            return a_kind >= SlopApi::ControlKind::Toggle &&
                   a_kind <= SlopApi::ControlKind::ButtonBinding;
        }

        [[nodiscard]] bool ValidValueKind(SlopApi::ValueKind a_kind) noexcept
        {
            return a_kind >= SlopApi::ValueKind::Boolean &&
                   a_kind <= SlopApi::ValueKind::String;
        }

        [[nodiscard]] SlopApi::ValueKind ExpectedValueKind(SlopApi::ControlKind a_kind) noexcept
        {
            switch (a_kind) {
            case SlopApi::ControlKind::Toggle: return SlopApi::ValueKind::Boolean;
            case SlopApi::ControlKind::IntegerSlider:
            case SlopApi::ControlKind::Choice: return SlopApi::ValueKind::Integer;
            case SlopApi::ControlKind::FloatSlider: return SlopApi::ValueKind::Float;
            case SlopApi::ControlKind::ButtonBinding:
            case SlopApi::ControlKind::Action: return SlopApi::ValueKind::String;
            }
            return SlopApi::ValueKind::String;
        }

        [[nodiscard]] bool Terminated(const SlopApi::ValueV1& a_value) noexcept
        {
            return std::memchr(a_value.stringValue, '\0', SlopApi::kStringValueCapacity) != nullptr;
        }

        [[nodiscard]] bool ValidDescriptor(const MenuApiHost::Control& a_control) noexcept
        {
            if (!ValidKind(a_control.kind)) return false;
            if (a_control.kind == SlopApi::ControlKind::Toggle || a_control.kind == SlopApi::ControlKind::Action ||
                a_control.kind == SlopApi::ControlKind::ButtonBinding) return true;
            const bool rangesAreFinite = std::isfinite(a_control.minimumValue) && std::isfinite(a_control.maximumValue) &&
                   std::isfinite(a_control.stepValue) && a_control.minimumValue <= a_control.maximumValue &&
                   a_control.stepValue > 0.0;
            if (!rangesAreFinite) return false;
            return (a_control.kind != SlopApi::ControlKind::IntegerSlider && a_control.kind != SlopApi::ControlKind::Choice) ||
                   (a_control.minimumValue >= -kMaximumExactScaleformInteger && a_control.maximumValue <= kMaximumExactScaleformInteger);
        }

        [[nodiscard]] bool ValidWrite(const MenuApiHost::Control& a_control,
            const SlopApi::ValueV1& a_value) noexcept
        {
            if (a_control.kind == SlopApi::ControlKind::Action || !ValidDescriptor(a_control) ||
                !ValidValueKind(a_value.kind) || a_value.kind != ExpectedValueKind(a_control.kind)) return false;
            if (a_value.kind == SlopApi::ValueKind::Float && !std::isfinite(a_value.floatValue)) return false;
            if (a_value.kind == SlopApi::ValueKind::String && !Terminated(a_value)) return false;
            if (a_control.kind == SlopApi::ControlKind::IntegerSlider || a_control.kind == SlopApi::ControlKind::Choice)
                return a_value.integerValue >= static_cast<std::int64_t>(a_control.minimumValue) &&
                       a_value.integerValue <= static_cast<std::int64_t>(a_control.maximumValue);
            if (a_control.kind == SlopApi::ControlKind::FloatSlider)
                return a_value.floatValue >= a_control.minimumValue && a_value.floatValue <= a_control.maximumValue;
            return true;
        }

        [[nodiscard]] const MenuApiHost::Page* Find(const std::vector<MenuApiHost::Page>& a_pages,
            std::string_view a_module, std::string_view a_page) noexcept
        {
            const auto it = std::ranges::find_if(a_pages, [&](const auto& candidate) {
                return candidate.moduleId == a_module && candidate.pageId == a_page;
            });
            return it == a_pages.end() ? nullptr : &*it;
        }
    }

    void Session::SetError(std::string_view a_error)
    {
        error_.assign(a_error.substr(0, kMaximumError));
    }

    bool Session::IsDirty() const noexcept { return !dirtyPageId_.empty(); }

    bool Session::IsDirtyOtherPage(const Command& a_command) const noexcept
    {
        return IsDirty() && (a_command.moduleId != dirtyModuleId_ || a_command.pageId != dirtyPageId_);
    }

    Model Session::BuildSnapshot()
    {
        Model model;
        model.revision = MenuApiHost::Revision();
        const auto pages = MenuApiHost::Pages();
        std::size_t totalControls{};
        for (const auto& source : pages) {
            if (model.pages.size() == kMaximumPages || source.controls.size() > kMaximumControlsPerPage ||
                totalControls + source.controls.size() > kMaximumControls) {
                SetError("Menu model capacity exceeded");
                break;
            }
            Page page{ source.moduleId, source.pageId, source.displayName, source.description };
            for (const auto& descriptor : source.controls) {
                Control control{ descriptor };
                control.value.kind = ExpectedValueKind(descriptor.kind);
                if (!ValidDescriptor(descriptor)) {
                    control.error = "Invalid control descriptor";
                } else if (descriptor.kind == SlopApi::ControlKind::Action) {
                    control.available = source.invokeAction != nullptr;
                    if (!control.available) control.error = "Action unavailable";
                } else {
                    SlopApi::ValueV1 value;
                    const auto result = source.readValue(source.context, descriptor.controlId.c_str(), &value);
                    if (result == SlopApi::Result::Ok && value.structSize >= sizeof(SlopApi::ValueV1) &&
                        ValidValueKind(value.kind) && value.kind == ExpectedValueKind(descriptor.kind) &&
                        (value.kind != SlopApi::ValueKind::Float || std::isfinite(value.floatValue)) &&
                        (value.kind != SlopApi::ValueKind::String || Terminated(value))) {
                        control.value = value;
                        control.available = true;
                    } else {
                        control.error = "Provider value unavailable";
                    }
                }
                page.controls.push_back(std::move(control));
                ++totalControls;
            }
            model.pages.push_back(std::move(page));
        }
        if (activePageId_.empty() && !model.pages.empty()) {
            activeModuleId_ = model.pages.front().moduleId;
            activePageId_ = model.pages.front().pageId;
        }
        model.activeModuleId = activeModuleId_;
        model.activePageId = activePageId_;
        model.selectedControlId = selectedControlId_;
        model.dirty = IsDirty();
        model.error = error_;
        return model;
    }

    Model Session::Snapshot() { return BuildSnapshot(); }

    Model Session::Dispatch(const Command& a_command)
    {
        error_.clear();
        if (a_command.schemaVersion != kSchemaVersion) {
            SetError("Unsupported command schema"); return BuildSnapshot();
        }
        const auto pages = MenuApiHost::Pages();
        const auto* page = Find(pages, a_command.moduleId, a_command.pageId);
        if (a_command.kind != CommandKind::Close && !page) {
            SetError("Unknown page"); return BuildSnapshot();
        }
        if (a_command.kind != CommandKind::Close && IsDirtyOtherPage(a_command)) {
            SetError("Apply or cancel the dirty page before navigation"); return BuildSnapshot();
        }
        if (a_command.kind == CommandKind::Close) {
            const auto* active = Find(pages, activeModuleId_, activePageId_);
            if (IsDirty()) {
                if (!active || active->moduleId != dirtyModuleId_ || active->pageId != dirtyPageId_ || !active->cancel) {
                    SetError("Dirty page cannot close without rollback"); return BuildSnapshot();
                }
                active->cancel(active->context);
                dirtyModuleId_.clear(); dirtyPageId_.clear(); selectedControlId_.clear();
            }
            return BuildSnapshot();
        }
        if (a_command.kind == CommandKind::SelectPage) {
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId; selectedControlId_.clear();
            return BuildSnapshot();
        }
        const auto controlIt = std::ranges::find_if(page->controls, [&](const auto& candidate) {
            return candidate.controlId == a_command.controlId;
        });
        if (a_command.kind == CommandKind::Apply || a_command.kind == CommandKind::Cancel) {
            if (a_command.controlId.empty()) {
                if (a_command.kind == CommandKind::Apply) {
                    if (!page->apply || page->apply(page->context) != SlopApi::Result::Ok) {
                        SetError("Apply failed"); return BuildSnapshot();
                    }
                } else if (page->cancel) {
                    page->cancel(page->context);
                }
                dirtyModuleId_.clear(); dirtyPageId_.clear(); selectedControlId_.clear();
                return BuildSnapshot();
            }
            SetError("Page command must not name a control"); return BuildSnapshot();
        }
        if (controlIt == page->controls.end()) { SetError("Unknown control"); return BuildSnapshot(); }
        const auto& control = *controlIt;
        if (a_command.kind == CommandKind::SelectControl) {
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId; selectedControlId_ = control.controlId;
        } else if (a_command.kind == CommandKind::Write) {
            if ((control.flags & SlopApi::kControlReadOnly) != 0 || !ValidWrite(control, a_command.value)) {
                SetError("Invalid control value"); return BuildSnapshot();
            }
            if (page->writeDraft(page->context, control.controlId.c_str(), &a_command.value) != SlopApi::Result::Ok) {
                SetError("Write failed"); return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId;
            selectedControlId_ = control.controlId;
            dirtyModuleId_ = page->moduleId; dirtyPageId_ = page->pageId;
        } else if (a_command.kind == CommandKind::Invoke) {
            if (control.kind != SlopApi::ControlKind::Action || !page->invokeAction ||
                page->invokeAction(page->context, control.controlId.c_str()) != SlopApi::Result::Ok) {
                SetError("Action unavailable");
            } else {
                activeModuleId_ = page->moduleId; activePageId_ = page->pageId;
                selectedControlId_ = control.controlId;
            }
        } else {
            SetError("Unknown command");
        }
        return BuildSnapshot();
    }
}
