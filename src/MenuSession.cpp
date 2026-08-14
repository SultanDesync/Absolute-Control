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
        constexpr std::size_t kMaximumError = AbsoluteControlPanelApi::kDescriptionCapacity - 1;
        constexpr double kMaximumExactScaleformInteger = 9007199254740991.0;

        [[nodiscard]] bool ValidKind(AbsoluteControlPanelApi::ControlKind a_kind) noexcept
        {
            return a_kind >= AbsoluteControlPanelApi::ControlKind::Toggle &&
                   a_kind <= AbsoluteControlPanelApi::ControlKind::ButtonBinding;
        }

        [[nodiscard]] bool ValidValueKind(AbsoluteControlPanelApi::ValueKind a_kind) noexcept
        {
            return a_kind >= AbsoluteControlPanelApi::ValueKind::Boolean &&
                   a_kind <= AbsoluteControlPanelApi::ValueKind::String;
        }

        [[nodiscard]] AbsoluteControlPanelApi::ValueKind ExpectedValueKind(AbsoluteControlPanelApi::ControlKind a_kind) noexcept
        {
            switch (a_kind) {
            case AbsoluteControlPanelApi::ControlKind::Toggle: return AbsoluteControlPanelApi::ValueKind::Boolean;
            case AbsoluteControlPanelApi::ControlKind::IntegerSlider:
            case AbsoluteControlPanelApi::ControlKind::Choice: return AbsoluteControlPanelApi::ValueKind::Integer;
            case AbsoluteControlPanelApi::ControlKind::FloatSlider: return AbsoluteControlPanelApi::ValueKind::Float;
            case AbsoluteControlPanelApi::ControlKind::ButtonBinding:
            case AbsoluteControlPanelApi::ControlKind::Action: return AbsoluteControlPanelApi::ValueKind::String;
            }
            return AbsoluteControlPanelApi::ValueKind::String;
        }

        [[nodiscard]] bool Terminated(const AbsoluteControlPanelApi::ValueV1& a_value) noexcept
        {
            return std::memchr(a_value.stringValue, '\0', AbsoluteControlPanelApi::kStringValueCapacity) != nullptr;
        }

        [[nodiscard]] bool ValidDescriptor(const MenuApiHost::Control& a_control) noexcept
        {
            if (!ValidKind(a_control.kind)) return false;
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::Toggle || a_control.kind == AbsoluteControlPanelApi::ControlKind::Action ||
                a_control.kind == AbsoluteControlPanelApi::ControlKind::ButtonBinding) return true;
            const bool rangesAreFinite = std::isfinite(a_control.minimumValue) && std::isfinite(a_control.maximumValue) &&
                   std::isfinite(a_control.stepValue) && a_control.minimumValue <= a_control.maximumValue &&
                   a_control.stepValue > 0.0;
            if (!rangesAreFinite) return false;
            return (a_control.kind != AbsoluteControlPanelApi::ControlKind::IntegerSlider && a_control.kind != AbsoluteControlPanelApi::ControlKind::Choice) ||
                   (a_control.minimumValue >= -kMaximumExactScaleformInteger && a_control.maximumValue <= kMaximumExactScaleformInteger);
        }

        [[nodiscard]] bool ValidWrite(const MenuApiHost::Control& a_control,
            const AbsoluteControlPanelApi::ValueV1& a_value) noexcept
        {
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::Action || !ValidDescriptor(a_control) ||
                !ValidValueKind(a_value.kind) || a_value.kind != ExpectedValueKind(a_control.kind)) return false;
            if (a_value.kind == AbsoluteControlPanelApi::ValueKind::Float && !std::isfinite(a_value.floatValue)) return false;
            if (a_value.kind == AbsoluteControlPanelApi::ValueKind::String && !Terminated(a_value)) return false;
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::IntegerSlider || a_control.kind == AbsoluteControlPanelApi::ControlKind::Choice)
                return a_value.integerValue >= static_cast<std::int64_t>(a_control.minimumValue) &&
                       a_value.integerValue <= static_cast<std::int64_t>(a_control.maximumValue);
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::FloatSlider)
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

    Session::~Session() noexcept
    {
        // The transaction token remains held during Cancel, so a concurrent
        // unregisterModule call cannot invalidate provider code mid-rollback.
        if (!RollbackDirtyPage()) {
            AbandonState();
        }
        captureModuleId_.clear();
        capturePageId_.clear();
        captureControlId_.clear();
        captureFlags_ = 0;
    }

    bool Session::RollbackDirtyPage() noexcept
    {
        if (!IsDirty()) return true;
        const auto page = MenuApiHost::FindPage(dirtyModuleId_, dirtyPageId_);
        if (!page || !page->canCancel ||
            MenuApiHost::Cancel(*page) != AbsoluteControlPanelApi::Result::Ok) {
            return false;
        }
        transaction_.Reset();
        dirtyModuleId_.clear();
        dirtyPageId_.clear();
        selectedControlId_.clear();
        return true;
    }

    void Session::AbandonState() noexcept
    {
        transaction_.Reset();
        dirtyModuleId_.clear();
        dirtyPageId_.clear();
        selectedControlId_.clear();
    }

    bool Session::IsBindingCaptureActive() const noexcept
    {
        return !captureControlId_.empty();
    }

    std::uint32_t Session::BindingCaptureFlags() const noexcept
    {
        return captureFlags_;
    }

    bool Session::IsDirtyOtherPage(const Command& a_command) const noexcept
    {
        return IsDirty() && (a_command.moduleId != dirtyModuleId_ || a_command.pageId != dirtyPageId_);
    }

    Model Session::BuildSnapshot()
    {
        Model model;
        model.generation = ++generation_;
        model.revision = MenuApiHost::Revision();
        const auto pages = MenuApiHost::Pages();
        std::size_t totalControls{};
        for (const auto& source : pages) {
            if (model.pages.size() == MenuApiHost::kMaximumPages ||
                source.controls.size() > MenuApiHost::kMaximumControlsPerPage ||
                totalControls + source.controls.size() > MenuApiHost::kMaximumControls) {
                SetError("Menu model capacity exceeded");
                break;
            }
            Page page{ source.moduleId, source.moduleDisplayName, source.pageId,
                source.displayName, source.description };
            for (const auto& descriptor : source.controls) {
                Control control{ descriptor };
                control.value.kind = ExpectedValueKind(descriptor.kind);
                if (!ValidDescriptor(descriptor)) {
                    control.error = "Invalid control descriptor";
                } else if (descriptor.kind == AbsoluteControlPanelApi::ControlKind::Action) {
                    control.available = source.canInvokeAction;
                    if (!control.available) control.error = "Action unavailable";
                } else {
                    AbsoluteControlPanelApi::ValueV1 value;
                    const auto result = MenuApiHost::ReadValue(
                        source, descriptor.controlId, value);
                    if (result == AbsoluteControlPanelApi::Result::Ok && value.structSize >= sizeof(AbsoluteControlPanelApi::ValueV1) &&
                        ValidValueKind(value.kind) && value.kind == ExpectedValueKind(descriptor.kind) &&
                        (value.kind != AbsoluteControlPanelApi::ValueKind::Float || std::isfinite(value.floatValue)) &&
                        (value.kind != AbsoluteControlPanelApi::ValueKind::String || Terminated(value))) {
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
        const auto activeExists = std::ranges::any_of(model.pages, [&](const Page& a_page) {
            return a_page.moduleId == activeModuleId_ && a_page.pageId == activePageId_;
        });
        if (!activeExists && !model.pages.empty()) {
            activeModuleId_ = model.pages.front().moduleId;
            activePageId_ = model.pages.front().pageId;
            selectedControlId_.clear();
        } else if (model.pages.empty()) {
            activeModuleId_.clear();
            activePageId_.clear();
            selectedControlId_.clear();
        }
        const auto captureExists = std::ranges::any_of(model.pages, [&](const Page& a_page) {
            return a_page.moduleId == captureModuleId_ && a_page.pageId == capturePageId_;
        });
        if (IsBindingCaptureActive() && !captureExists) {
            captureModuleId_.clear(); capturePageId_.clear(); captureControlId_.clear();
            captureFlags_ = 0;
            SetError("Input capture provider was unregistered");
        }
        model.activeModuleId = activeModuleId_;
        model.activePageId = activePageId_;
        model.selectedControlId = selectedControlId_;
        model.dirty = IsDirty();
        model.bindingCaptureActive = IsBindingCaptureActive();
        model.captureModuleId = captureModuleId_;
        model.capturePageId = capturePageId_;
        model.captureControlId = captureControlId_;
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
        if (a_command.expectedGeneration != 0 &&
            a_command.expectedGeneration != generation_) {
            SetError("Stale menu command"); return BuildSnapshot();
        }
        if (IsBindingCaptureActive() &&
            a_command.kind != CommandKind::BeginBindingCapture) {
            SetError("Finish or cancel input capture first");
            return BuildSnapshot();
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
            if (IsDirty() && !RollbackDirtyPage()) {
                SetError("Dirty page cannot close without rollback");
                return BuildSnapshot();
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
                    if (MenuApiHost::Apply(*page) != AbsoluteControlPanelApi::Result::Ok) {
                        SetError("Apply failed"); return BuildSnapshot();
                    }
                    transaction_.Reset();
                    dirtyModuleId_.clear(); dirtyPageId_.clear(); selectedControlId_.clear();
                } else if (IsDirty()) {
                    if (!RollbackDirtyPage()) {
                        SetError("Cancel failed"); return BuildSnapshot();
                    }
                } else {
                    if (MenuApiHost::Cancel(*page) !=
                        AbsoluteControlPanelApi::Result::Ok) {
                        SetError("Cancel failed"); return BuildSnapshot();
                    }
                    selectedControlId_.clear();
                }
                return BuildSnapshot();
            }
            SetError("Page command must not name a control"); return BuildSnapshot();
        }
        if (controlIt == page->controls.end()) { SetError("Unknown control"); return BuildSnapshot(); }
        const auto& control = *controlIt;
        if (a_command.kind == CommandKind::SelectControl) {
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId; selectedControlId_ = control.controlId;
        } else if (a_command.kind == CommandKind::Write) {
            if ((control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0 || !ValidWrite(control, a_command.value)) {
                SetError("Invalid control value"); return BuildSnapshot();
            }
            if (MenuApiHost::WriteDraft(*page, control.controlId,
                    a_command.value, transaction_) != AbsoluteControlPanelApi::Result::Ok) {
                SetError("Write failed"); return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId;
            selectedControlId_ = control.controlId;
            dirtyModuleId_ = page->moduleId; dirtyPageId_ = page->pageId;
        } else if (a_command.kind == CommandKind::BeginBindingCapture) {
            constexpr auto kDeviceMask = AbsoluteControlPanelApi::kBindingKeyboard |
                AbsoluteControlPanelApi::kBindingMouse | AbsoluteControlPanelApi::kBindingController;
            if (control.kind != AbsoluteControlPanelApi::ControlKind::ButtonBinding ||
                (control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0 ||
                (control.flags & kDeviceMask) == 0) {
                SetError("Input capture unavailable");
                return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId;
            activePageId_ = page->pageId;
            selectedControlId_ = control.controlId;
            captureModuleId_ = page->moduleId;
            capturePageId_ = page->pageId;
            captureControlId_ = control.controlId;
            captureFlags_ = control.flags;
        } else if (a_command.kind == CommandKind::Invoke) {
            if (control.kind != AbsoluteControlPanelApi::ControlKind::Action ||
                MenuApiHost::InvokeAction(*page, control.controlId) !=
                    AbsoluteControlPanelApi::Result::Ok) {
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

    Model Session::CompleteBindingCapture(std::string_view a_binding)
    {
        if (!IsBindingCaptureActive() ||
            a_binding.size() >= AbsoluteControlPanelApi::kStringValueCapacity) {
            SetError("No active input capture");
            return BuildSnapshot();
        }
        Command command;
        command.kind = CommandKind::Write;
        command.moduleId = captureModuleId_;
        command.pageId = capturePageId_;
        command.controlId = captureControlId_;
        command.value.kind = AbsoluteControlPanelApi::ValueKind::String;
        std::memcpy(command.value.stringValue, a_binding.data(), a_binding.size());
        command.value.stringValue[a_binding.size()] = '\0';
        captureModuleId_.clear();
        capturePageId_.clear();
        captureControlId_.clear();
        captureFlags_ = 0;
        return Dispatch(command);
    }

    Model Session::CancelBindingCapture(std::string_view a_reason)
    {
        captureModuleId_.clear();
        capturePageId_.clear();
        captureControlId_.clear();
        captureFlags_ = 0;
        error_.clear();
        if (!a_reason.empty()) {
            SetError(a_reason);
        }
        return BuildSnapshot();
    }
}
