#include "MenuSession.h"

#include "LiveComponentsRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <ranges>
#include <utility>

namespace AbsoluteControlPanelResearch::MenuSession
{
    namespace
    {
        constexpr std::size_t kMaximumError = AbsoluteControlPanelApi::kDescriptionCapacity - 1;
        constexpr double kMaximumExactScaleformInteger = 9007199254740991.0;

        [[nodiscard]] bool ValidKind(AbsoluteControlPanelApi::ControlKind a_kind) noexcept
        {
            return a_kind >= AbsoluteControlPanelApi::ControlKind::Toggle &&
                   a_kind <= AbsoluteControlPanelApi::ControlKind::TextInput;
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
            case AbsoluteControlPanelApi::ControlKind::TextInput:
            case AbsoluteControlPanelApi::ControlKind::Action: return AbsoluteControlPanelApi::ValueKind::String;
            }
            return AbsoluteControlPanelApi::ValueKind::String;
        }

        [[nodiscard]] bool Terminated(const AbsoluteControlPanelApi::ValueV1& a_value) noexcept
        {
            return std::memchr(a_value.stringValue, '\0', AbsoluteControlPanelApi::kStringValueCapacity) != nullptr;
        }

        template <std::size_t Size>
        void Copy(char (&target)[Size], std::string_view source) noexcept
        {
            std::ranges::fill(target, '\0');
            const auto count = (std::min)(source.size(), Size - 1);
            std::memcpy(target, source.data(), count);
        }

        [[nodiscard]] bool ValidDescriptor(const MenuApiHost::Control& a_control) noexcept
        {
            if (!ValidKind(a_control.kind)) return false;
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::Toggle || a_control.kind == AbsoluteControlPanelApi::ControlKind::Action ||
                a_control.kind == AbsoluteControlPanelApi::ControlKind::ButtonBinding) return true;
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::TextInput) {
                return std::isfinite(a_control.minimumValue) &&
                       std::isfinite(a_control.maximumValue) &&
                       std::isfinite(a_control.stepValue) &&
                       a_control.minimumValue == 0.0 &&
                       a_control.maximumValue >= 1.0 &&
                       a_control.maximumValue <
                           AbsoluteControlPanelApi::kStringValueCapacity &&
                       std::floor(a_control.maximumValue) == a_control.maximumValue &&
                       a_control.stepValue == 1.0;
            }
            const bool rangesAreFinite = std::isfinite(a_control.minimumValue) && std::isfinite(a_control.maximumValue) &&
                   std::isfinite(a_control.stepValue) && a_control.minimumValue <= a_control.maximumValue &&
                   a_control.stepValue > 0.0;
            if (!rangesAreFinite) return false;
            return (a_control.kind != AbsoluteControlPanelApi::ControlKind::IntegerSlider && a_control.kind != AbsoluteControlPanelApi::ControlKind::Choice) ||
                   (a_control.minimumValue >= -kMaximumExactScaleformInteger && a_control.maximumValue <= kMaximumExactScaleformInteger);
        }

        [[nodiscard]] bool ValidValue(const MenuApiHost::Control& a_control,
            const AbsoluteControlPanelApi::ValueV1& a_value) noexcept
        {
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::Action || !ValidDescriptor(a_control) ||
                !ValidValueKind(a_value.kind) || a_value.kind != ExpectedValueKind(a_control.kind)) return false;
            if (a_value.kind == AbsoluteControlPanelApi::ValueKind::Float && !std::isfinite(a_value.floatValue)) return false;
            if (a_value.kind == AbsoluteControlPanelApi::ValueKind::String && !Terminated(a_value)) return false;
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::TextInput &&
                strnlen_s(a_value.stringValue,
                    AbsoluteControlPanelApi::kStringValueCapacity) >
                    static_cast<std::size_t>(a_control.maximumValue)) return false;
            if (a_value.kind == AbsoluteControlPanelApi::ValueKind::Boolean &&
                a_value.booleanValue > 1) return false;
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::IntegerSlider || a_control.kind == AbsoluteControlPanelApi::ControlKind::Choice)
                return a_value.integerValue >= static_cast<std::int64_t>(a_control.minimumValue) &&
                       a_value.integerValue <= static_cast<std::int64_t>(a_control.maximumValue);
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::FloatSlider)
                return a_value.floatValue >= a_control.minimumValue && a_value.floatValue <= a_control.maximumValue;
            return true;
        }

        void SynthesizeChoiceOptions(const MenuApiHost::Control& a_control,
            std::vector<MenuApiHost::ChoiceOption>& a_options)
        {
            if (a_control.kind != AbsoluteControlPanelApi::ControlKind::Choice ||
                std::floor(a_control.minimumValue) != a_control.minimumValue ||
                std::floor(a_control.maximumValue) != a_control.maximumValue ||
                std::floor(a_control.stepValue) != a_control.stepValue ||
                a_control.stepValue < 1.0) {
                return;
            }
            const auto minimum = static_cast<std::int64_t>(a_control.minimumValue);
            const auto maximum = static_cast<std::int64_t>(a_control.maximumValue);
            const auto step = static_cast<std::int64_t>(a_control.stepValue);
            for (auto value = minimum; value <= maximum;) {
                if (a_options.size() >=
                    AbsoluteControlPanelApi::kMaximumChoiceOptions) {
                    a_options.clear();
                    return;
                }
                a_options.push_back({ value, std::to_string(value) });
                if (maximum - value < step) break;
                value += step;
            }
        }

    }

    void Session::SetError(std::string_view a_error)
    {
        error_.assign(a_error.substr(0, kMaximumError));
    }

    bool Session::IsDirty() const noexcept { return !dirtyPageId_.empty(); }

    Session::~Session() noexcept
    {
        Teardown();
    }

    void Session::Teardown() noexcept
    {
        // The transaction token remains held during Cancel, so a concurrent
        // unregisterModule call cannot invalidate provider code mid-rollback.
        if (!RollbackDirtyPage()) {
            AbandonState();
        }
        ClearCapture();
        ClearDirtyDecision();
        closeRequested_ = false;
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
        ClearDirtyDecision();
    }

    bool Session::IsBindingCaptureActive() const noexcept
    {
        return captureKind_ == CaptureKind::Binding && !captureControlId_.empty();
    }

    bool Session::IsTextCaptureActive() const noexcept
    {
        return captureKind_ == CaptureKind::Text && !captureControlId_.empty();
    }

    bool Session::IsCaptureActive() const noexcept
    {
        return captureKind_ != CaptureKind::None && !captureControlId_.empty();
    }

    std::uint32_t Session::BindingCaptureFlags() const noexcept
    {
        return captureFlags_;
    }

    std::string_view Session::ActiveModuleId() const noexcept
    {
        return activeModuleId_;
    }

    std::string_view Session::ActivePageId() const noexcept
    {
        return activePageId_;
    }

    void Session::AcknowledgePublishedGeneration(
        std::uint64_t a_generation) noexcept
    {
        if (a_generation >= publishedGeneration_ && a_generation <= generation_) {
            publishedGeneration_ = a_generation;
        }
    }

    bool Session::IsDirtyOtherPage(const Command& a_command) const noexcept
    {
        return IsDirty() && (a_command.moduleId != dirtyModuleId_ || a_command.pageId != dirtyPageId_);
    }

    void Session::BeginDirtyDecision(const Command& a_command)
    {
        dirtyDecisionKind_ = a_command.kind == CommandKind::Close ?
            DirtyDecisionKind::Close : DirtyDecisionKind::Navigate;
        decisionModuleId_ = a_command.moduleId;
        decisionPageId_ = a_command.pageId;
    }

    void Session::ClearDirtyDecision() noexcept
    {
        dirtyDecisionKind_ = DirtyDecisionKind::None;
        decisionModuleId_.clear();
        decisionPageId_.clear();
    }

    void Session::CompletePendingRoute()
    {
        if (dirtyDecisionKind_ == DirtyDecisionKind::Close) {
            closeRequested_ = true;
        } else if (dirtyDecisionKind_ == DirtyDecisionKind::Navigate) {
            const auto page = MenuApiHost::FindPage(
                decisionModuleId_, decisionPageId_);
            if (!page) {
                SetError("The requested page is no longer available");
                ClearDirtyDecision();
                return;
            }
            activeModuleId_ = page->moduleId;
            activePageId_ = page->pageId;
            selectedControlId_.clear();
        }
        ClearDirtyDecision();
    }

    Model Session::ResolveDirtyDecision(CommandKind a_kind)
    {
        if (dirtyDecisionKind_ == DirtyDecisionKind::None) {
            SetError("No pending dirty-page decision");
            return BuildSnapshot();
        }
        if (a_kind == CommandKind::ResolveDirtyStay) {
            ClearDirtyDecision();
            return BuildSnapshot();
        }

        const auto page = MenuApiHost::FindPage(dirtyModuleId_, dirtyPageId_);
        if (!page) {
            SetError("The dirty provider page is no longer available");
            return BuildSnapshot();
        }
        if (a_kind == CommandKind::ResolveDirtyApply) {
            if (!page->canApply ||
                MenuApiHost::Apply(*page) !=
                    AbsoluteControlPanelApi::Result::Ok) {
                SetError("Apply failed; your draft is still available");
                return BuildSnapshot();
            }
            transaction_.Reset();
            dirtyModuleId_.clear();
            dirtyPageId_.clear();
        } else if (a_kind == CommandKind::ResolveDirtyDiscard) {
            if (!RollbackDirtyPage()) {
                SetError("Discard failed; your draft is still available");
                return BuildSnapshot();
            }
        } else {
            SetError("Unknown dirty-page decision");
            return BuildSnapshot();
        }
        CompletePendingRoute();
        return BuildSnapshot();
    }

    Model Session::BuildSnapshot()
    {
        Model model;
        model.generation = ++generation_;
        const auto catalog =
            MenuApiHost::SnapshotCatalog(activeModuleId_, activePageId_);
        model.revision = catalog.revision;
        model.modules.reserve(catalog.modules.size());
        for (const auto& source : catalog.modules) {
            model.modules.push_back(Module{
                source.moduleId, source.displayName, source.firstPageId });
        }
        model.pages.reserve(catalog.pages.size());
        for (const auto& source : catalog.pages) {
            Page page{ source.moduleId, source.moduleDisplayName, source.pageId,
                source.displayName, source.description };
            page.controls.reserve(source.controls.size());
            for (const auto& descriptor : source.controls) {
                Control control{ descriptor };
                control.value.kind = ExpectedValueKind(descriptor.kind);
                if (!ValidDescriptor(descriptor)) {
                    control.error = "Invalid control descriptor";
                } else if (descriptor.kind == AbsoluteControlPanelApi::ControlKind::Action) {
                    control.available = source.canInvokeAction &&
                        (descriptor.flags & AbsoluteControlPanelApi::kControlReadOnly) == 0;
                    if (!control.available) control.error = "Action unavailable";
                } else {
                    AbsoluteControlPanelApi::ValueV1 value;
                    const auto result = MenuApiHost::ReadValue(
                        source, descriptor.controlId, value);
                    if (result == AbsoluteControlPanelApi::Result::Ok &&
                        value.structSize >= sizeof(AbsoluteControlPanelApi::ValueV1) &&
                        ValidValue(descriptor, value)) {
                        control.value = value;
                        control.available = true;
                    } else {
                        control.error = "Provider value unavailable";
                    }
                }
                if (control.available && descriptor.kind ==
                        AbsoluteControlPanelApi::ControlKind::Choice) {
                    const auto optionsResult = MenuApiHost::ReadChoiceOptions(
                        source, descriptor.controlId, control.choiceOptions);
                    if (optionsResult == AbsoluteControlPanelApi::Result::NotFound) {
                        SynthesizeChoiceOptions(descriptor, control.choiceOptions);
                    }
                    const bool validOptions = !control.choiceOptions.empty() &&
                        std::ranges::all_of(control.choiceOptions,
                            [&](const MenuApiHost::ChoiceOption& option) {
                                return option.value >= static_cast<std::int64_t>(
                                    descriptor.minimumValue) &&
                                    option.value <= static_cast<std::int64_t>(
                                    descriptor.maximumValue);
                            }) &&
                        std::ranges::any_of(control.choiceOptions,
                            [&](const MenuApiHost::ChoiceOption& option) {
                                return option.value == control.value.integerValue;
                            });
                    if (!validOptions) {
                        control.available = false;
                        control.error = "Choice options unavailable";
                        control.choiceOptions.clear();
                    }
                }
                page.controls.push_back(std::move(control));
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
        auto& liveRegistry = LiveComponents::HostRegistry();
        if (!activeModuleId_.empty() && !activePageId_.empty()) {
            (void)liveRegistry.SetVisiblePage(
                activeModuleId_.c_str(), activePageId_.c_str());
            (void)liveRegistry.PollVisiblePage();
            const auto publications = liveRegistry.SnapshotPage(
                activeModuleId_.c_str(), activePageId_.c_str());
            const auto activePage = std::ranges::find_if(
                model.pages, [&](const Page& page) {
                    return page.moduleId == activeModuleId_ &&
                           page.pageId == activePageId_;
                });
            if (activePage != model.pages.end()) {
                activePage->liveComponents.reserve(publications.size());
                for (const auto& publication : publications) {
                    Page::LiveComponent component;
                    component.descriptor = publication.channel;
                    component.available = publication.publish != 0;
                    if (component.available) component.frame = publication.frame;
                    else component.error = "Live component is waiting for provider data";
                    activePage->liveComponents.push_back(std::move(component));
                }
            }
        }
        bool captureExists = !IsCaptureActive();
        if (IsCaptureActive()) {
            if (const auto capturePage =
                    MenuApiHost::FindPage(captureModuleId_, capturePageId_)) {
                const auto captureControl = std::ranges::find(
                    capturePage->controls, captureControlId_,
                    &MenuApiHost::Control::controlId);
                captureExists = captureControl != capturePage->controls.end() &&
                    ((IsBindingCaptureActive() && captureControl->kind ==
                        AbsoluteControlPanelApi::ControlKind::ButtonBinding) ||
                     (IsTextCaptureActive() && captureControl->kind ==
                        AbsoluteControlPanelApi::ControlKind::TextInput));
            }
        }
        if (IsCaptureActive() && !captureExists) {
            ClearCapture();
            SetError("Input capture provider was unregistered");
        }
        if (IsTextCaptureActive()) {
            const auto capturePage = std::ranges::find_if(
                model.pages, [&](const Page& page) {
                    return page.moduleId == captureModuleId_ &&
                           page.pageId == capturePageId_;
                });
            if (capturePage != model.pages.end()) {
                const auto captureControl = std::ranges::find_if(
                    capturePage->controls, [&](const Control& control) {
                        return control.descriptor.controlId == captureControlId_;
                    });
                if (captureControl != capturePage->controls.end()) {
                    captureControl->value.kind =
                        AbsoluteControlPanelApi::ValueKind::String;
                    Copy(captureControl->value.stringValue, captureBuffer_);
                    captureControl->available = true;
                }
            }
        }
        model.activeModuleId = activeModuleId_;
        model.activePageId = activePageId_;
        model.selectedControlId = selectedControlId_;
        model.dirty = IsDirty();
        model.dirtyDecisionActive =
            dirtyDecisionKind_ != DirtyDecisionKind::None;
        model.dirtyDecisionClosesMenu =
            dirtyDecisionKind_ == DirtyDecisionKind::Close;
        model.closeRequested = std::exchange(closeRequested_, false);
        model.bindingCaptureActive = IsBindingCaptureActive();
        model.textCaptureActive = IsTextCaptureActive();
        model.captureModuleId = captureModuleId_;
        model.capturePageId = capturePageId_;
        model.captureControlId = captureControlId_;
        model.error = error_;
        return model;
    }

    Model Session::Snapshot() { return BuildSnapshot(); }

    std::optional<Model> Session::RefreshLive()
    {
        if (activeModuleId_.empty() || activePageId_.empty()) return std::nullopt;
        auto& registry = LiveComponents::HostRegistry();
        if (!registry.HasPageChannels(
                activeModuleId_.c_str(), activePageId_.c_str()) ||
            !registry.PollVisiblePage()) {
            return std::nullopt;
        }
        return BuildSnapshot();
    }

    Model Session::Dispatch(const Command& a_command)
    {
        error_.clear();
        if (a_command.schemaVersion != kSchemaVersion) {
            SetError("Unsupported command schema"); return BuildSnapshot();
        }
        if (a_command.expectedGeneration != 0 &&
            a_command.expectedGeneration != publishedGeneration_) {
            SetError("Stale menu command"); return BuildSnapshot();
        }
        const bool resolvesDirtyDecision =
            a_command.kind == CommandKind::ResolveDirtyApply ||
            a_command.kind == CommandKind::ResolveDirtyDiscard ||
            a_command.kind == CommandKind::ResolveDirtyStay;
        if (dirtyDecisionKind_ != DirtyDecisionKind::None) {
            if (!resolvesDirtyDecision) {
                SetError("Choose Apply, Discard, or Stay first");
                return BuildSnapshot();
            }
            return ResolveDirtyDecision(a_command.kind);
        }
        if (resolvesDirtyDecision) {
            SetError("No pending dirty-page decision");
            return BuildSnapshot();
        }
        if (IsCaptureActive() &&
            a_command.kind != CommandKind::BeginBindingCapture &&
            a_command.kind != CommandKind::BeginTextCapture) {
            SetError("Finish or cancel input capture first");
            return BuildSnapshot();
        }
        const auto page = MenuApiHost::FindPage(
            a_command.moduleId, a_command.pageId);
        if (a_command.kind != CommandKind::Close && !page) {
            SetError("Unknown page"); return BuildSnapshot();
        }
        if (a_command.kind != CommandKind::Close && IsDirtyOtherPage(a_command)) {
            if (a_command.kind == CommandKind::SelectPage) {
                BeginDirtyDecision(a_command);
                return BuildSnapshot();
            }
            SetError("The command does not own the dirty page");
            return BuildSnapshot();
        }
        if (a_command.kind == CommandKind::Close) {
            if (IsDirty()) {
                BeginDirtyDecision(a_command);
                return BuildSnapshot();
            }
            closeRequested_ = true;
            return BuildSnapshot();
        }
        if (a_command.kind == CommandKind::SelectPage) {
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId; selectedControlId_.clear();
            return BuildSnapshot();
        }
        if (a_command.kind == CommandKind::Compound) {
            const bool alreadyAttached = static_cast<bool>(transaction_);
            if (!page->canApply || !page->canCancel ||
                MenuApiHost::AttachTransaction(*page, transaction_) !=
                    AbsoluteControlPanelApi::Result::Ok) {
                SetError("Compound editor transaction unavailable");
                return BuildSnapshot();
            }
            AbsoluteControlPanelExperimental::CompoundOperationV1 operation;
            operation.kind = a_command.compoundKind;
            Copy(operation.moduleId, page->moduleId);
            Copy(operation.pageId, page->pageId);
            Copy(operation.channelId, a_command.channelId);
            Copy(operation.controlId, a_command.controlId);
            Copy(operation.columnId, a_command.columnId);
            Copy(operation.tierId, a_command.tierId);
            operation.count = a_command.count;
            const auto result = LiveComponents::HostRegistry().Apply(operation);
            if (result.result != AbsoluteControlPanelExperimental::Result::Ok) {
                if (!alreadyAttached) {
                    (void)MenuApiHost::Cancel(*page);
                    transaction_.Reset();
                }
                SetError("Compound edit rejected by provider");
                return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId;
            activePageId_ = page->pageId;
            selectedControlId_ = a_command.controlId;
            dirtyModuleId_ = page->moduleId;
            dirtyPageId_ = page->pageId;
            return BuildSnapshot();
        }
        const auto controlIt = std::ranges::find_if(page->controls, [&](const auto& candidate) {
            return candidate.controlId == a_command.controlId;
        });
        if (a_command.kind == CommandKind::Apply || a_command.kind == CommandKind::Cancel) {
            if (a_command.controlId.empty()) {
                if (!IsDirty()) {
                    SetError("No pending changes");
                    return BuildSnapshot();
                }
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
            if ((control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0 || !ValidValue(control, a_command.value)) {
                SetError("Invalid control value"); return BuildSnapshot();
            }
            const bool transientChoice = control.kind ==
                    AbsoluteControlPanelApi::ControlKind::Choice &&
                (control.flags &
                    AbsoluteControlPanelApi::kControlTransientChoice) != 0;
            const auto writeResult = transientChoice ?
                MenuApiHost::WriteTransientChoice(
                    *page, control.controlId, a_command.value) :
                MenuApiHost::WriteDraft(*page, control.controlId,
                    a_command.value, transaction_);
            if (writeResult != AbsoluteControlPanelApi::Result::Ok) {
                SetError("Write failed"); return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId;
            selectedControlId_ = control.controlId;
            if (!transientChoice) {
                dirtyModuleId_ = page->moduleId; dirtyPageId_ = page->pageId;
            }
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
            captureKind_ = CaptureKind::Binding;
        } else if (a_command.kind == CommandKind::BeginTextCapture) {
            AbsoluteControlPanelApi::ValueV1 opening;
            if (control.kind != AbsoluteControlPanelApi::ControlKind::TextInput ||
                (control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0 ||
                MenuApiHost::ReadValue(*page, control.controlId, opening) !=
                    AbsoluteControlPanelApi::Result::Ok ||
                opening.kind != AbsoluteControlPanelApi::ValueKind::String ||
                !Terminated(opening)) {
                SetError("Text editing unavailable");
                return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId;
            activePageId_ = page->pageId;
            selectedControlId_ = control.controlId;
            captureModuleId_ = page->moduleId;
            capturePageId_ = page->pageId;
            captureControlId_ = control.controlId;
            captureKind_ = CaptureKind::Text;
            captureBuffer_ = opening.stringValue;
            captureMaximum_ = static_cast<std::size_t>(control.maximumValue);
        } else if (a_command.kind == CommandKind::Invoke) {
            const bool mutatesDraft =
                (control.flags & AbsoluteControlPanelApi::kControlMutatesDraft) != 0;
            const bool appliesDraftBeforeInvoke =
                (control.flags &
                    AbsoluteControlPanelApi::kControlAppliesDraftBeforeInvoke) != 0;
            if (control.kind != AbsoluteControlPanelApi::ControlKind::Action ||
                (control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0) {
                SetError("Action unavailable");
            } else {
                if (appliesDraftBeforeInvoke && IsDirty()) {
                    if (!page->canApply ||
                        MenuApiHost::Apply(*page) !=
                            AbsoluteControlPanelApi::Result::Ok) {
                        SetError("Apply before action failed");
                        return BuildSnapshot();
                    }
                    transaction_.Reset();
                    dirtyModuleId_.clear();
                    dirtyPageId_.clear();
                }
                const bool alreadyAttached = static_cast<bool>(transaction_);
                if (mutatesDraft &&
                    (!page->canApply || !page->canCancel ||
                     MenuApiHost::AttachTransaction(*page, transaction_) !=
                         AbsoluteControlPanelApi::Result::Ok)) {
                    SetError("Action transaction unavailable");
                    return BuildSnapshot();
                }
                if (MenuApiHost::InvokeAction(*page, control.controlId) !=
                    AbsoluteControlPanelApi::Result::Ok) {
                    if (mutatesDraft && !alreadyAttached) {
                        (void)MenuApiHost::Cancel(*page);
                        transaction_.Reset();
                    }
                    SetError("Action unavailable");
                } else {
                    activeModuleId_ = page->moduleId; activePageId_ = page->pageId;
                    selectedControlId_ = control.controlId;
                    if (mutatesDraft) {
                        dirtyModuleId_ = page->moduleId;
                        dirtyPageId_ = page->pageId;
                    }
                }
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
        ClearCapture();
        return Dispatch(command);
    }

    Model Session::CancelBindingCapture(std::string_view a_reason)
    {
        ClearCapture();
        error_.clear();
        if (!a_reason.empty()) {
            SetError(a_reason);
        }
        return BuildSnapshot();
    }

    Model Session::AppendTextCapture(char a_character)
    {
        error_.clear();
        const auto byte = static_cast<unsigned char>(a_character);
        if (!IsTextCaptureActive() || byte < 0x20 || byte > 0x7E) {
            SetError("No active text edit");
        } else if (captureBuffer_.size() < captureMaximum_) {
            captureBuffer_.push_back(a_character);
        }
        return BuildSnapshot();
    }

    Model Session::BackspaceTextCapture()
    {
        error_.clear();
        if (!IsTextCaptureActive()) {
            SetError("No active text edit");
        } else if (!captureBuffer_.empty()) {
            captureBuffer_.pop_back();
        }
        return BuildSnapshot();
    }

    Model Session::CompleteTextCapture()
    {
        if (!IsTextCaptureActive() || captureBuffer_.empty() ||
            captureBuffer_.size() > captureMaximum_) {
            SetError("Text value is empty or invalid");
            return BuildSnapshot();
        }
        Command command;
        command.kind = CommandKind::Write;
        command.moduleId = captureModuleId_;
        command.pageId = capturePageId_;
        command.controlId = captureControlId_;
        command.value.kind = AbsoluteControlPanelApi::ValueKind::String;
        Copy(command.value.stringValue, captureBuffer_);
        ClearCapture();
        return Dispatch(command);
    }

    Model Session::CancelTextCapture(std::string_view a_reason)
    {
        ClearCapture();
        error_.clear();
        if (!a_reason.empty()) SetError(a_reason);
        return BuildSnapshot();
    }

    void Session::ClearCapture() noexcept
    {
        captureModuleId_.clear();
        capturePageId_.clear();
        captureControlId_.clear();
        captureFlags_ = 0;
        captureKind_ = CaptureKind::None;
        captureBuffer_.clear();
        captureMaximum_ = 0;
    }
}
