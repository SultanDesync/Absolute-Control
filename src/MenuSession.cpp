#include "MenuSession.h"

#include "LiveComponentsRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <ranges>
#include <span>
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
                   a_kind <= AbsoluteControlPanelApi::ControlKind::RecordCollection;
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
            case AbsoluteControlPanelApi::ControlKind::RecordCollection:
            case AbsoluteControlPanelApi::ControlKind::Action:
            case AbsoluteControlPanelApi::ControlKind::GroupHeader:
                return AbsoluteControlPanelApi::ValueKind::String;
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
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::Toggle ||
                a_control.kind == AbsoluteControlPanelApi::ControlKind::Action ||
                a_control.kind == AbsoluteControlPanelApi::ControlKind::ButtonBinding ||
                a_control.kind == AbsoluteControlPanelApi::ControlKind::GroupHeader ||
                a_control.kind == AbsoluteControlPanelApi::ControlKind::RecordCollection) return true;
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
            if (a_control.kind == AbsoluteControlPanelApi::ControlKind::Action ||
                a_control.kind == AbsoluteControlPanelApi::ControlKind::GroupHeader ||
                !ValidDescriptor(a_control) ||
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

    void Session::BeginBindingConflict(std::string_view a_moduleId,
        std::string_view a_pageId, std::string_view a_controlId,
        std::string_view a_binding, std::string_view a_detail)
    {
        ClearCapture();
        conflictModuleId_ = a_moduleId;
        conflictPageId_ = a_pageId;
        conflictControlId_ = a_controlId;
        conflictBinding_.assign(a_binding.substr(
            0, AbsoluteControlPanelApi::kStringValueCapacity - 1));
        conflictDetail_.assign(a_detail.substr(0, kMaximumError));
        if (conflictDetail_.empty()) {
            conflictDetail_ = "This binding is already assigned to another profile.";
        }
        activeModuleId_ = conflictModuleId_;
        activePageId_ = conflictPageId_;
        selectedControlId_ = conflictControlId_;
        error_.clear();
    }

    void Session::ClearBindingConflict() noexcept
    {
        conflictModuleId_.clear();
        conflictPageId_.clear();
        conflictControlId_.clear();
        conflictBinding_.clear();
        conflictDetail_.clear();
    }

    void Session::BeginActionConfirmation(const MenuApiHost::Page& a_page,
        const MenuApiHost::Control& a_control)
    {
        confirmModuleId_ = a_page.moduleId;
        confirmPageId_ = a_page.pageId;
        confirmControlId_ = a_control.controlId;
        confirmLabel_ = a_control.label;
        confirmDetail_ = a_control.description;
        activeModuleId_ = a_page.moduleId;
        activePageId_ = a_page.pageId;
        selectedControlId_ = a_control.controlId;
    }

    void Session::ClearActionConfirmation() noexcept
    {
        confirmModuleId_.clear();
        confirmPageId_.clear();
        confirmControlId_.clear();
        confirmLabel_.clear();
        confirmDetail_.clear();
    }

    void Session::ExecuteAction(const MenuApiHost::Page& a_page,
        const MenuApiHost::Control& a_control)
    {
        const bool mutatesDraft =
            (a_control.flags & AbsoluteControlPanelApi::kControlMutatesDraft) != 0;
        const bool appliesDraftBeforeInvoke =
            (a_control.flags &
                AbsoluteControlPanelApi::kControlAppliesDraftBeforeInvoke) != 0;
        if (a_control.kind != AbsoluteControlPanelApi::ControlKind::Action ||
            (a_control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0) {
            SetError("Action unavailable");
            return;
        }
        if (appliesDraftBeforeInvoke && IsDirty()) {
            if (!a_page.canApply ||
                MenuApiHost::Apply(a_page) != AbsoluteControlPanelApi::Result::Ok) {
                SetError("Apply before action failed");
                return;
            }
            transaction_.Reset();
            dirtyModuleId_.clear();
            dirtyPageId_.clear();
        }
        const bool alreadyAttached = static_cast<bool>(transaction_);
        if (mutatesDraft &&
            (!a_page.canApply || !a_page.canCancel ||
             MenuApiHost::AttachTransaction(a_page, transaction_) !=
                 AbsoluteControlPanelApi::Result::Ok)) {
            SetError("Action transaction unavailable");
            return;
        }
        if (MenuApiHost::InvokeAction(a_page, a_control.controlId) !=
            AbsoluteControlPanelApi::Result::Ok) {
            if (mutatesDraft && !alreadyAttached) {
                (void)MenuApiHost::Cancel(a_page);
                transaction_.Reset();
            }
            SetError("Action unavailable");
            return;
        }
        activeModuleId_ = a_page.moduleId;
        activePageId_ = a_page.pageId;
        selectedControlId_ = a_control.controlId;
        if (mutatesDraft) {
            dirtyModuleId_ = a_page.moduleId;
            dirtyPageId_ = a_page.pageId;
        }
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
        ClearBindingConflict();
        ClearActionConfirmation();
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
        selectedGridColumnId_.clear();
        return true;
    }

    void Session::AbandonState() noexcept
    {
        transaction_.Reset();
        dirtyModuleId_.clear();
        dirtyPageId_.clear();
        selectedControlId_.clear();
        selectedGridColumnId_.clear();
        ClearDirtyDecision();
        ClearActionConfirmation();
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
                } else if (descriptor.kind ==
                    AbsoluteControlPanelApi::ControlKind::GroupHeader) {
                    control.available = true;
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
                if (control.available && descriptor.kind ==
                        AbsoluteControlPanelApi::ControlKind::RecordCollection) {
                    const auto recordsResult = MenuApiHost::ReadRecordItems(
                        source, descriptor.controlId, control.recordItems);
                    const std::string_view selected{ control.value.stringValue };
                    const bool selectedExists = selected.empty() ||
                        std::ranges::any_of(control.recordItems,
                            [&](const MenuApiHost::RecordItem& item) {
                                return item.recordId == selected;
                            });
                    if (recordsResult != AbsoluteControlPanelApi::Result::Ok ||
                        !selectedExists) {
                        control.available = false;
                        control.error = "Record collection unavailable";
                        control.recordItems.clear();
                    }
                }
                page.controls.push_back(std::move(control));
            }
            if (!source.controls.empty()) {
                auto composition =
                    Composition::HostRegistry().Snapshot(source);
                page.composition = std::move(composition.model);
                if (page.composition.enhanced) {
                    for (auto& control : page.controls) {
                        const auto* placement =
                            Composition::FindControlPlacement(
                                page.composition,
                                control.descriptor.controlId);
                        if (!placement) continue;
                        control.semanticVisible =
                            Composition::IsEffectivelyVisible(
                                page.composition, *placement);
                        control.semanticEnabled =
                            Composition::IsEffectivelyEnabled(
                                page.composition, *placement);
                        if (!control.semanticEnabled) {
                            control.available = false;
                            if (control.error.empty()) {
                                control.error =
                                    "Disabled by current composition state";
                            }
                        }
                    }
                }
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
            selectedGridColumnId_.clear();
        }
        const auto activeSemanticPage = std::ranges::find_if(
            model.pages, [&](const Page& page) {
                return page.moduleId == activeModuleId_ &&
                       page.pageId == activePageId_;
            });
        if (activeSemanticPage != model.pages.end() &&
            activeSemanticPage->composition.enhanced) {
            const auto order = Composition::SelectableControlOrder(
                activeSemanticPage->composition);
            if (std::ranges::find(order, selectedControlId_) == order.end()) {
                selectedControlId_ = order.empty() ? std::string{} : order.front();
            }
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
                    std::uint32_t retainedAssociations{};
                    for (std::uint32_t associationIndex = 0;
                         associationIndex <
                            component.descriptor.associationCount;
                         ++associationIndex) {
                        const auto& association =
                            component.descriptor.associations[associationIndex];
                        const auto target = std::ranges::find(
                            activePage->controls, association.controlId,
                            [](const Control& control) {
                                return control.descriptor.controlId;
                            });
                        if (target == activePage->controls.end() ||
                            target->descriptor.kind !=
                                AbsoluteControlPanelApi::ControlKind::Choice) {
                            continue;
                        }
                        component.descriptor.associations[
                            retainedAssociations++] = association;
                    }
                    component.descriptor.associationCount =
                        retainedAssociations;
                    component.available = publication.publish != 0;
                    if (component.available) component.frame = publication.frame;
                    else component.error = "Live component is waiting for provider data";
                    if (component.descriptor.kind ==
                            AbsoluteControlPanelExperimental::ComponentKind::TelemetryPlot) {
                        (void)liveRegistry.TelemetryHistory(
                            activeModuleId_.c_str(), activePageId_.c_str(),
                            component.descriptor.channelId,
                            component.telemetryHistory);
                    }
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
        model.selectedGridColumnId = selectedGridColumnId_;
        model.dirty = IsDirty();
        model.dirtyDecisionActive =
            dirtyDecisionKind_ != DirtyDecisionKind::None;
        model.dirtyDecisionClosesMenu =
            dirtyDecisionKind_ == DirtyDecisionKind::Close;
        model.closeRequested = std::exchange(closeRequested_, false);
        model.bindingCaptureActive = IsBindingCaptureActive();
        model.textCaptureActive = IsTextCaptureActive();
        model.bindingConflictActive = !conflictControlId_.empty();
        model.actionConfirmationActive = !confirmControlId_.empty();
        model.captureModuleId = captureModuleId_;
        model.capturePageId = capturePageId_;
        model.captureControlId = captureControlId_;
        model.bindingConflictDetail = conflictDetail_;
        model.actionConfirmationLabel = confirmLabel_;
        model.actionConfirmationDetail = confirmDetail_;
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

    std::optional<LivePatch> Session::RefreshLivePatch()
    {
        if (activeModuleId_.empty() || activePageId_.empty()) {
            return std::nullopt;
        }
        auto& registry = LiveComponents::HostRegistry();
        if (!registry.HasPageChannels(
                activeModuleId_.c_str(), activePageId_.c_str()) ||
            !registry.PollVisiblePage()) {
            return std::nullopt;
        }

        LivePatch patch;
        patch.moduleId = activeModuleId_;
        patch.pageId = activePageId_;
        const auto publications = registry.SnapshotPage(
            activeModuleId_.c_str(), activePageId_.c_str());
        patch.components.reserve(publications.size());
        for (const auto& publication : publications) {
            Page::LiveComponent component;
            component.descriptor = publication.channel;
            component.available = publication.publish != 0;
            if (component.available) {
                component.frame = publication.frame;
            } else {
                component.error =
                    "Live component is waiting for provider data";
            }
            patch.components.push_back(std::move(component));
        }
        return patch;
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
        if (!confirmControlId_.empty()) {
            if (a_command.kind == CommandKind::ResolveActionCancel) {
                ClearActionConfirmation();
                return BuildSnapshot();
            }
            if (a_command.kind != CommandKind::ResolveActionConfirm) {
                SetError("Choose Confirm or Cancel first");
                return BuildSnapshot();
            }
            const auto moduleId = confirmModuleId_;
            const auto pageId = confirmPageId_;
            const auto controlId = confirmControlId_;
            ClearActionConfirmation();
            const auto confirmPage = MenuApiHost::FindPage(moduleId, pageId);
            if (!confirmPage) {
                SetError("Action provider is unavailable");
                return BuildSnapshot();
            }
            const auto confirmControl = std::ranges::find(
                confirmPage->controls, controlId, &MenuApiHost::Control::controlId);
            if (confirmControl == confirmPage->controls.end() ||
                (confirmControl->flags &
                    AbsoluteControlPanelApi::kControlRequiresConfirmation) == 0) {
                SetError("Action confirmation is stale");
                return BuildSnapshot();
            }
            ExecuteAction(*confirmPage, *confirmControl);
            return BuildSnapshot();
        }
        if (a_command.kind == CommandKind::ResolveActionConfirm ||
            a_command.kind == CommandKind::ResolveActionCancel) {
            SetError("No pending action confirmation");
            return BuildSnapshot();
        }
        if (!conflictControlId_.empty()) {
            if (a_command.kind == CommandKind::ResolveBindingCancel) {
                ClearBindingConflict();
                return BuildSnapshot();
            }
            if (a_command.kind != CommandKind::ResolveBindingReassign) {
                SetError("Choose Reassign or Cancel first");
                return BuildSnapshot();
            }
            const auto conflictPage = MenuApiHost::FindPage(
                conflictModuleId_, conflictPageId_);
            if (!conflictPage || !conflictPage->canApply ||
                !conflictPage->canCancel) {
                SetError("Binding conflict provider is unavailable");
                return BuildSnapshot();
            }
            const auto result = MenuApiHost::ReassignBinding(
                *conflictPage, conflictControlId_, conflictBinding_, transaction_);
            if (result != AbsoluteControlPanelApi::Result::Ok) {
                SetError(result == AbsoluteControlPanelApi::Result::NotFound ?
                    "Provider does not support binding reassignment" :
                    "Binding reassignment failed");
                return BuildSnapshot();
            }
            activeModuleId_ = conflictModuleId_;
            activePageId_ = conflictPageId_;
            selectedControlId_ = conflictControlId_;
            dirtyModuleId_ = conflictModuleId_;
            dirtyPageId_ = conflictPageId_;
            ClearBindingConflict();
            return BuildSnapshot();
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
            selectedGridColumnId_.clear();
            return BuildSnapshot();
        }
        if (a_command.kind == CommandKind::SelectGridColumn) {
            AbsoluteControlPanelExperimental::LiveChannelModelV1 channel;
            if (LiveComponents::HostRegistry().Describe(
                    page->moduleId.c_str(), page->pageId.c_str(),
                    a_command.channelId.c_str(), channel) !=
                    AbsoluteControlPanelExperimental::Result::Ok ||
                channel.kind != AbsoluteControlPanelExperimental::ComponentKind::
                    SegmentedAllocationGrid ||
                std::ranges::none_of(
                    std::span{channel.segmentedGrid.columns,
                        channel.segmentedGrid.columnCount},
                    [&](const auto& column) {
                        return std::string_view{column.columnId} ==
                            a_command.columnId;
                    })) {
                SetError("Unknown grid row");
                return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId;
            activePageId_ = page->pageId;
            selectedControlId_.clear();
            selectedGridColumnId_ = a_command.columnId;
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
            selectedGridColumnId_ = a_command.columnId;
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
            if (control.kind == AbsoluteControlPanelApi::ControlKind::GroupHeader) {
                SetError("Section headers cannot receive focus");
                return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId; selectedControlId_ = control.controlId;
            selectedGridColumnId_.clear();
        } else if (a_command.kind == CommandKind::Write) {
            auto writeValue = a_command.value;
            writeValue.structSize = sizeof(AbsoluteControlPanelApi::ValueV1);
            if ((control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0 || !ValidValue(control, writeValue)) {
                SetError("Invalid control value"); return BuildSnapshot();
            }
            const bool transientSelection =
                (control.kind == AbsoluteControlPanelApi::ControlKind::Choice ||
                 control.kind == AbsoluteControlPanelApi::ControlKind::RecordCollection) &&
                (control.flags &
                    AbsoluteControlPanelApi::kControlTransientChoice) != 0;
            if (control.kind ==
                    AbsoluteControlPanelApi::ControlKind::RecordCollection) {
                std::vector<MenuApiHost::RecordItem> records;
                const auto recordsResult = MenuApiHost::ReadRecordItems(
                    *page, control.controlId, records);
                const auto selected = std::ranges::find(
                    records, std::string_view{ writeValue.stringValue },
                    &MenuApiHost::RecordItem::recordId);
                if (recordsResult != AbsoluteControlPanelApi::Result::Ok ||
                    selected == records.end() ||
                    (selected->flags &
                        AbsoluteControlPanelApi::kRecordItemDisabled) != 0) {
                    SetError("Invalid record selection");
                    return BuildSnapshot();
                }
            }
            const auto writeResult = transientSelection ?
                MenuApiHost::WriteTransientChoice(*page, control.controlId, writeValue) :
                MenuApiHost::WriteDraft(*page, control.controlId, writeValue, transaction_);
            if (writeResult != AbsoluteControlPanelApi::Result::Ok) {
                if (writeResult == AbsoluteControlPanelApi::Result::Duplicate &&
                    control.kind == AbsoluteControlPanelApi::ControlKind::InputBinding &&
                    a_command.value.kind == AbsoluteControlPanelApi::ValueKind::String &&
                    a_command.value.stringValue[0] != '\0') {
                    BeginBindingConflict(page->moduleId, page->pageId,
                        control.controlId, a_command.value.stringValue,
                        "This binding is already assigned to another profile.");
                    return BuildSnapshot();
                }
                SetError("Write failed"); return BuildSnapshot();
            }
            activeModuleId_ = page->moduleId; activePageId_ = page->pageId;
            selectedControlId_ = control.controlId;
            if (!transientSelection) {
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
            const bool keyboardCapture =
                (control.flags & AbsoluteControlPanelApi::kBindingKeyboard) != 0;
            const bool providerCapture =
                (control.flags & (AbsoluteControlPanelApi::kBindingMouse |
                    AbsoluteControlPanelApi::kBindingController)) != 0;
            providerCaptureActive_ = false;
            if (providerCapture) {
                const auto result = MenuApiHost::BeginBindingCapture(
                    *page, control.controlId);
                if (result == AbsoluteControlPanelApi::Result::Ok) {
                    providerCaptureActive_ = true;
                } else if (!keyboardCapture) {
                    SetError(result == AbsoluteControlPanelApi::Result::NotReady
                        ? "Input service is not ready" :
                        (result == AbsoluteControlPanelApi::Result::NotFound
                            ? "Provider capture unavailable" :
                            "Input service is busy or rejected the request"));
                    return BuildSnapshot();
                }
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
            if (control.kind != AbsoluteControlPanelApi::ControlKind::Action ||
                (control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0) {
                SetError("Action unavailable");
            } else if ((control.flags &
                    AbsoluteControlPanelApi::kControlRequiresConfirmation) != 0) {
                BeginActionConfirmation(*page, control);
            } else {
                ExecuteAction(*page, control);
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

    std::optional<Model> Session::RefreshBindingCapture()
    {
        if (!IsBindingCaptureActive() || !providerCaptureActive_) {
            return std::nullopt;
        }
        const auto page = MenuApiHost::FindPage(
            captureModuleId_, capturePageId_);
        if (!page) {
            return CancelBindingCapture("Binding provider disappeared during capture");
        }
        AbsoluteControlPanelApi::BindingCaptureV1 capture;
        const auto result = MenuApiHost::PollBindingCapture(
            *page, captureControlId_, capture);
        if (result != AbsoluteControlPanelApi::Result::Ok) {
            return CancelBindingCapture("Binding provider capture failed");
        }
        using State = AbsoluteControlPanelApi::BindingCaptureState;
        if (capture.state == State::Idle || capture.state == State::Capturing) {
            return std::nullopt;
        }
        const bool bindingTerminated =
            std::memchr(capture.binding, '\0', sizeof(capture.binding)) != nullptr;
        const bool detailTerminated =
            std::memchr(capture.detail, '\0', sizeof(capture.detail)) != nullptr;
        providerCaptureActive_ = false;
        if (capture.state == State::Captured && bindingTerminated &&
            capture.binding[0] != '\0') {
            return CompleteBindingCapture(capture.binding);
        }
        const std::string_view detail = detailTerminated
            ? std::string_view{ capture.detail } : std::string_view{};
        if (capture.state == State::Error && bindingTerminated &&
            capture.binding[0] != '\0') {
            const auto moduleId = captureModuleId_;
            const auto pageId = capturePageId_;
            const auto controlId = captureControlId_;
            BeginBindingConflict(moduleId, pageId, controlId,
                capture.binding, detail);
            return BuildSnapshot();
        }
        if (capture.state == State::Cancelled) {
            return CancelBindingCapture(detail);
        }
        return CancelBindingCapture(detail.empty()
            ? "Provider input capture did not complete" : detail);
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
        if (providerCaptureActive_) {
            if (const auto page = MenuApiHost::FindPage(
                    captureModuleId_, capturePageId_)) {
                (void)MenuApiHost::CancelBindingCapture(
                    *page, captureControlId_);
            }
        }
        providerCaptureActive_ = false;
        captureModuleId_.clear();
        capturePageId_.clear();
        captureControlId_.clear();
        captureFlags_ = 0;
        captureKind_ = CaptureKind::None;
        captureBuffer_.clear();
        captureMaximum_ = 0;
    }
}
