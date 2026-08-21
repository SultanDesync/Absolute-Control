#include "MenuInputRouter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ranges>

namespace AbsoluteControlPanelResearch::MenuInputRouter
{
    namespace
    {
        constexpr std::uint32_t kActionCount = 3;

        [[nodiscard]] bool IsUp(std::int32_t a_keyCode) noexcept
        {
            return a_keyCode == kUp || a_keyCode == kArrowUp;
        }

        [[nodiscard]] bool IsDown(std::int32_t a_keyCode) noexcept
        {
            return a_keyCode == kDown || a_keyCode == kArrowDown;
        }

        [[nodiscard]] bool IsLeft(std::int32_t a_keyCode) noexcept
        {
            return a_keyCode == kLeft || a_keyCode == kArrowLeft;
        }

        [[nodiscard]] bool IsRight(std::int32_t a_keyCode) noexcept
        {
            return a_keyCode == kRight || a_keyCode == kArrowRight;
        }

        [[nodiscard]] bool IsAccept(std::int32_t a_keyCode) noexcept
        {
            return a_keyCode == kAccept || a_keyCode == kEnter || a_keyCode == kSpace;
        }

        [[nodiscard]] std::size_t ActivePageIndex(const MenuSession::Model& a_model) noexcept
        {
            const auto found = std::ranges::find_if(a_model.pages, [&](const auto& page) {
                return page.moduleId == a_model.activeModuleId &&
                       page.pageId == a_model.activePageId;
            });
            return found == a_model.pages.end() ? 0 :
                static_cast<std::size_t>(std::distance(a_model.pages.begin(), found));
        }

        [[nodiscard]] std::size_t ActiveModuleIndex(
            const MenuSession::Model& a_model) noexcept
        {
            const auto found = std::ranges::find_if(
                a_model.modules, [&](const MenuSession::Module& a_module) {
                    return a_module.moduleId == a_model.activeModuleId;
                });
            return found == a_model.modules.end() ? 0 :
                static_cast<std::size_t>(
                    std::distance(a_model.modules.begin(), found));
        }

        [[nodiscard]] std::size_t SelectedControlIndex(
            const MenuSession::Model& a_model, const MenuSession::Page& a_page) noexcept
        {
            const auto found = std::ranges::find_if(a_page.controls, [&](const auto& control) {
                return control.descriptor.controlId == a_model.selectedControlId;
            });
            if (found != a_page.controls.end() && found->semanticVisible &&
                found->semanticEnabled) {
                return static_cast<std::size_t>(
                    std::distance(a_page.controls.begin(), found));
            }
            const auto first = std::ranges::find_if(
                a_page.controls, [](const auto& control) {
                    return control.descriptor.kind !=
                               SlopApi::ControlKind::GroupHeader &&
                           control.semanticVisible &&
                           control.semanticEnabled;
                });
            return first == a_page.controls.end() ? 0 :
                static_cast<std::size_t>(
                    std::distance(a_page.controls.begin(), first));
        }

        [[nodiscard]] const MenuSession::Page::LiveComponent* GridComponent(
            const MenuSession::Page& a_page) noexcept
        {
            const auto found = std::ranges::find_if(
                a_page.liveComponents, [](const auto& component) {
                    return component.available && component.descriptor.kind ==
                        AbsoluteControlPanelExperimental::ComponentKind::
                            SegmentedAllocationGrid;
                });
            return found == a_page.liveComponents.end() ? nullptr : &*found;
        }

        [[nodiscard]] std::size_t SelectedGridColumnIndex(
            const MenuSession::Model& a_model,
            const MenuSession::Page::LiveComponent& a_component) noexcept
        {
            const auto& grid = a_component.descriptor.segmentedGrid;
            for (std::size_t index{}; index < grid.columnCount; ++index) {
                if (std::string_view{grid.columns[index].columnId} ==
                    a_model.selectedGridColumnId) return index;
            }
            return 0;
        }

        [[nodiscard]] MenuSession::Command GridSelectionCommand(
            const MenuSession::Page& a_page,
            const MenuSession::Page::LiveComponent& a_component,
            std::size_t a_column)
        {
            MenuSession::Command command;
            command.kind = MenuSession::CommandKind::SelectGridColumn;
            command.moduleId = a_page.moduleId;
            command.pageId = a_page.pageId;
            command.channelId = a_component.descriptor.channelId;
            command.columnId = a_component.descriptor.segmentedGrid
                .columns[a_column].columnId;
            return command;
        }

        [[nodiscard]] std::optional<MenuSession::Command> AdjustGrid(
            const MenuSession::Model& a_model, const MenuSession::Page& a_page,
            const MenuSession::Page::LiveComponent& a_component,
            std::int32_t a_direction) noexcept
        {
            using namespace AbsoluteControlPanelExperimental;
            const auto& descriptor = a_component.descriptor.segmentedGrid;
            const auto& frame = a_component.frame.segmentedGrid;
            if (descriptor.columnCount == 0 || descriptor.tierCount < 2 ||
                frame.columnCount != descriptor.columnCount) return std::nullopt;
            const auto columnIndex = (std::min)(
                SelectedGridColumnIndex(a_model, a_component),
                static_cast<std::size_t>(descriptor.columnCount - 1));
            const auto& source = frame.columns[columnIndex];
            std::uint32_t filled{};
            std::uint32_t firstTierCount{};
            bool interactive{};
            for (std::uint32_t index{}; index < source.segmentCount; ++index) {
                const auto& segment = source.segments[index];
                interactive = interactive || segment.interactive != 0;
                if (segment.tierIndex != 0) ++filled;
                if (segment.tierIndex == 1) ++firstTierCount;
            }
            if (!interactive) return std::nullopt;
            MenuSession::Command command;
            command.kind = MenuSession::CommandKind::Compound;
            command.moduleId = a_page.moduleId;
            command.pageId = a_page.pageId;
            command.channelId = a_component.descriptor.channelId;
            command.controlId = descriptor.controlId;
            command.columnId = descriptor.columns[columnIndex].columnId;
            if (a_direction < 0) {
                if (filled == 0) return std::nullopt;
                command.compoundKind = CompoundOperationKind::TrimColumn;
                command.count = filled - 1;
            } else {
                if (filled >= descriptor.columns[columnIndex].maximumSegments) {
                    return std::nullopt;
                }
                command.compoundKind = CompoundOperationKind::SetSegmentCount;
                command.tierId = descriptor.tiers[1].tierId;
                command.count = firstTierCount + 1;
            }
            return command;
        }

        [[nodiscard]] MenuSession::Command PageCommand(
            MenuSession::CommandKind a_kind, const MenuSession::Page& a_page)
        {
            MenuSession::Command command;
            command.kind = a_kind;
            command.moduleId = a_page.moduleId;
            command.pageId = a_page.pageId;
            return command;
        }

        [[nodiscard]] MenuSession::Command ModuleCommand(
            const MenuSession::Module& a_module)
        {
            MenuSession::Command command;
            command.kind = MenuSession::CommandKind::SelectPage;
            command.moduleId = a_module.moduleId;
            command.pageId = a_module.firstPageId;
            return command;
        }

        [[nodiscard]] MenuSession::Command ControlCommand(
            MenuSession::CommandKind a_kind, const MenuSession::Page& a_page,
            const MenuSession::Control& a_control)
        {
            auto command = PageCommand(a_kind, a_page);
            command.controlId = a_control.descriptor.controlId;
            return command;
        }

        [[nodiscard]] bool Selectable(
            const MenuSession::Control& control) noexcept
        {
            return control.descriptor.kind !=
                       SlopApi::ControlKind::GroupHeader &&
                   control.semanticVisible && control.semanticEnabled;
        }

        [[nodiscard]] std::optional<MenuSession::Command> AnchorCommand(
            const MenuSession::Page& page,
            const Composition::AnchorTarget& anchor) noexcept
        {
            const auto control = std::ranges::find(
                page.controls, anchor.controlId,
                [](const MenuSession::Control& candidate) {
                    return candidate.descriptor.controlId;
                });
            if (control == page.controls.end() || !Selectable(*control)) {
                return std::nullopt;
            }
            return ControlCommand(
                MenuSession::CommandKind::SelectControl, page, *control);
        }

        [[nodiscard]] std::optional<MenuSession::Command> Adjust(
            const MenuSession::Page& a_page, const MenuSession::Control& a_control,
            std::int32_t a_direction) noexcept
        {
            const auto& descriptor = a_control.descriptor;
            if (!a_control.available || (descriptor.flags & SlopApi::kControlReadOnly) != 0) {
                return std::nullopt;
            }
            auto command = ControlCommand(MenuSession::CommandKind::Write, a_page, a_control);
            if (descriptor.kind == SlopApi::ControlKind::IntegerSlider ||
                descriptor.kind == SlopApi::ControlKind::Choice) {
                const auto step = (std::max)(std::int64_t{ 1 },
                    static_cast<std::int64_t>(std::llround(descriptor.stepValue)));
                command.value.kind = SlopApi::ValueKind::Integer;
                command.value.integerValue = std::clamp(
                    a_control.value.integerValue + a_direction * step,
                    static_cast<std::int64_t>(descriptor.minimumValue),
                    static_cast<std::int64_t>(descriptor.maximumValue));
                return command;
            }
            if (descriptor.kind == SlopApi::ControlKind::FloatSlider) {
                command.value.kind = SlopApi::ValueKind::Float;
                command.value.floatValue = std::clamp(
                    a_control.value.floatValue + a_direction * descriptor.stepValue,
                    descriptor.minimumValue, descriptor.maximumValue);
                return command;
            }
            if (descriptor.kind == SlopApi::ControlKind::RecordCollection &&
                !a_control.recordItems.empty()) {
                const auto selected = std::ranges::find(
                    a_control.recordItems,
                    std::string_view{ a_control.value.stringValue },
                    &MenuApiHost::RecordItem::recordId);
                const auto count = a_control.recordItems.size();
                auto index = selected == a_control.recordItems.end() ?
                    (a_direction < 0 ? 0U : count - 1) :
                    static_cast<std::size_t>(std::distance(
                        a_control.recordItems.begin(), selected));
                for (std::size_t attempts{}; attempts < count; ++attempts) {
                    index = a_direction < 0 ? (index + count - 1) % count :
                        (index + 1) % count;
                    const auto& item = a_control.recordItems[index];
                    if ((item.flags &
                            AbsoluteControlPanelApi::kRecordItemDisabled) != 0) {
                        continue;
                    }
                    command.value.kind = SlopApi::ValueKind::String;
                    std::ranges::fill(command.value.stringValue, '\0');
                    std::memcpy(command.value.stringValue,
                        item.recordId.data(), item.recordId.size());
                    return command;
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<MenuSession::Command> Activate(
            const MenuSession::Page& a_page, const MenuSession::Control& a_control) noexcept
        {
            if (!a_control.available ||
                (a_control.descriptor.flags & SlopApi::kControlReadOnly) != 0) {
                return std::nullopt;
            }
            if (a_control.descriptor.kind == SlopApi::ControlKind::Toggle) {
                auto command = ControlCommand(MenuSession::CommandKind::Write, a_page, a_control);
                command.value.kind = SlopApi::ValueKind::Boolean;
                command.value.booleanValue = a_control.value.booleanValue == 0 ? 1U : 0U;
                return command;
            }
            if (a_control.descriptor.kind == SlopApi::ControlKind::Action) {
                return ControlCommand(MenuSession::CommandKind::Invoke, a_page, a_control);
            }
            if (a_control.descriptor.kind == SlopApi::ControlKind::ButtonBinding) {
                return ControlCommand(
                    MenuSession::CommandKind::BeginBindingCapture, a_page, a_control);
            }
            if (a_control.descriptor.kind == SlopApi::ControlKind::TextInput) {
                return ControlCommand(
                    MenuSession::CommandKind::BeginTextCapture, a_page, a_control);
            }
            return Adjust(a_page, a_control, 1);
        }

        [[nodiscard]] MenuSession::Command ActionCommand(
            const MenuSession::Page& a_page, std::uint32_t a_actionIndex)
        {
            if (a_actionIndex == 0) {
                return PageCommand(MenuSession::CommandKind::Apply, a_page);
            }
            if (a_actionIndex == 1) {
                return PageCommand(MenuSession::CommandKind::Cancel, a_page);
            }
            MenuSession::Command command;
            command.kind = MenuSession::CommandKind::Close;
            return command;
        }

    }

    bool IsMenuKey(std::int32_t a_keyCode) noexcept
    {
        return a_keyCode == kEscape || a_keyCode == kTab ||
               IsAccept(a_keyCode) || IsUp(a_keyCode) ||
               IsDown(a_keyCode) || IsLeft(a_keyCode) || IsRight(a_keyCode) ||
               a_keyCode == kDecrease || a_keyCode == kIncrease ||
               a_keyCode == kPreviousPage || a_keyCode == kNextPage ||
               a_keyCode == kApply || a_keyCode == kCancel;
    }

    RouteResult Route(const MenuSession::Model& a_model, std::int32_t a_keyCode,
        FocusState a_focus) noexcept
    {
        RouteResult result{ .handled = IsMenuKey(a_keyCode), .focus = a_focus };
        if (result.focus.region != FocusRegion::Anchors) {
            result.focus.actionIndex =
                (std::min)(result.focus.actionIndex, kActionCount - 1);
        }
        if (!result.handled) {
            return result;
        }
        if (a_model.actionConfirmationActive) {
            result.focus.region = FocusRegion::Actions;
            result.focus.actionIndex = (std::min)(result.focus.actionIndex, 1U);
            const auto ConfirmationCommand = [](MenuSession::CommandKind a_kind) {
                MenuSession::Command command;
                command.kind = a_kind;
                return command;
            };
            if (a_keyCode == kEscape || a_keyCode == kTab ||
                a_keyCode == kCancel) {
                result.command = ConfirmationCommand(
                    MenuSession::CommandKind::ResolveActionCancel);
            } else if (IsUp(a_keyCode) || IsLeft(a_keyCode) ||
                IsDown(a_keyCode) || IsRight(a_keyCode)) {
                result.focus.actionIndex = 1U - result.focus.actionIndex;
            } else if (IsAccept(a_keyCode)) {
                result.command = ConfirmationCommand(
                    result.focus.actionIndex == 0 ?
                        MenuSession::CommandKind::ResolveActionConfirm :
                        MenuSession::CommandKind::ResolveActionCancel);
            }
            return result;
        }
        if (a_model.dirtyDecisionActive) {
            result.focus.region = FocusRegion::Actions;
            const auto DecisionCommand = [](MenuSession::CommandKind a_kind) {
                MenuSession::Command command;
                command.kind = a_kind;
                return command;
            };
            if (a_keyCode == kEscape || a_keyCode == kTab) {
                result.command = DecisionCommand(
                    MenuSession::CommandKind::ResolveDirtyStay);
            } else if (a_keyCode == kApply) {
                result.command = DecisionCommand(
                    MenuSession::CommandKind::ResolveDirtyApply);
            } else if (a_keyCode == kCancel) {
                result.command = DecisionCommand(
                    MenuSession::CommandKind::ResolveDirtyDiscard);
            } else if (IsUp(a_keyCode) || IsDown(a_keyCode) ||
                IsLeft(a_keyCode) || IsRight(a_keyCode)) {
                result.focus.actionIndex =
                    (IsUp(a_keyCode) || IsLeft(a_keyCode)) ?
                    (result.focus.actionIndex + kActionCount - 1) % kActionCount :
                    (result.focus.actionIndex + 1) % kActionCount;
            } else if (IsAccept(a_keyCode)) {
                constexpr MenuSession::CommandKind decisions[]{
                    MenuSession::CommandKind::ResolveDirtyApply,
                    MenuSession::CommandKind::ResolveDirtyDiscard,
                    MenuSession::CommandKind::ResolveDirtyStay,
                };
                result.command = DecisionCommand(
                    decisions[result.focus.actionIndex]);
            }
            return result;
        }
        if (a_model.bindingConflictActive) {
            result.focus.region = FocusRegion::Actions;
            result.focus.actionIndex = (std::min)(result.focus.actionIndex, 1U);
            const auto ConflictCommand = [](MenuSession::CommandKind a_kind) {
                MenuSession::Command command;
                command.kind = a_kind;
                return command;
            };
            if (a_keyCode == kEscape || a_keyCode == kTab ||
                a_keyCode == kCancel) {
                result.command = ConflictCommand(
                    MenuSession::CommandKind::ResolveBindingCancel);
            } else if (IsUp(a_keyCode) || IsLeft(a_keyCode) ||
                IsDown(a_keyCode) || IsRight(a_keyCode)) {
                result.focus.actionIndex = 1U - result.focus.actionIndex;
            } else if (IsAccept(a_keyCode)) {
                result.command = ConflictCommand(result.focus.actionIndex == 0 ?
                    MenuSession::CommandKind::ResolveBindingReassign :
                    MenuSession::CommandKind::ResolveBindingCancel);
            }
            return result;
        }
        if (a_keyCode == kEscape || a_keyCode == kTab) {
            MenuSession::Command command;
            command.kind = MenuSession::CommandKind::Close;
            result.command = std::move(command);
            return result;
        }
        if (a_model.pages.empty()) {
            return result;
        }

        const auto pageIndex = (std::min)(ActivePageIndex(a_model), a_model.pages.size() - 1);
        const auto& page = a_model.pages[pageIndex];
        const auto* grid = GridComponent(page);
        const auto anchors = Composition::AnchorTargets(page.composition);
        if (result.focus.region == FocusRegion::Anchors) {
            if (anchors.empty()) {
                result.focus.region = FocusRegion::Controls;
                result.focus.actionIndex = 0;
            } else {
                result.focus.actionIndex = (std::min)(result.focus.actionIndex,
                    static_cast<std::uint32_t>(anchors.size() - 1));
            }
        }
        if (a_keyCode == kApply || a_keyCode == kCancel) {
            result.command = PageCommand(a_keyCode == kApply ?
                MenuSession::CommandKind::Apply : MenuSession::CommandKind::Cancel, page);
            return result;
        }
        if (a_keyCode == kPreviousPage || a_keyCode == kNextPage) {
            const auto pageCount = a_model.pages.size();
            const auto target = a_keyCode == kPreviousPage ?
                (pageIndex + pageCount - 1) % pageCount :
                (pageIndex + 1) % pageCount;
            result.focus.region = FocusRegion::Controls;
            result.command = PageCommand(
                MenuSession::CommandKind::SelectPage, a_model.pages[target]);
            return result;
        }

        if (IsLeft(a_keyCode) || IsRight(a_keyCode)) {
            if (result.focus.region == FocusRegion::Grid && grid) {
                result.command = AdjustGrid(a_model, page, *grid,
                    IsLeft(a_keyCode) ? -1 : 1);
            } else if (IsRight(a_keyCode) && result.focus.region == FocusRegion::Modules) {
                result.focus.region = grid ? FocusRegion::Grid :
                    (!anchors.empty() ? FocusRegion::Anchors :
                        FocusRegion::Controls);
                result.focus.actionIndex = 0;
            } else if (IsRight(a_keyCode) &&
                result.focus.region == FocusRegion::Anchors) {
                result.focus.region = FocusRegion::Controls;
            } else if (IsLeft(a_keyCode) && result.focus.region == FocusRegion::Controls) {
                result.focus.region = grid ? FocusRegion::Grid :
                    (!anchors.empty() ? FocusRegion::Anchors :
                        FocusRegion::Modules);
                result.focus.actionIndex = 0;
            } else if (IsLeft(a_keyCode) &&
                result.focus.region == FocusRegion::Anchors) {
                result.focus.region = FocusRegion::Modules;
            } else if (IsRight(a_keyCode) && result.focus.region == FocusRegion::Controls) {
                result.focus.region = FocusRegion::Actions;
                result.focus.actionIndex = 0;
            } else if (IsLeft(a_keyCode) && result.focus.region == FocusRegion::Actions) {
                result.focus.region = FocusRegion::Controls;
            }
            return result;
        }

        if (result.focus.region == FocusRegion::Anchors) {
            if (anchors.empty()) {
                result.focus.region = FocusRegion::Controls;
                return result;
            }
            if (IsUp(a_keyCode) || IsDown(a_keyCode)) {
                const auto count = static_cast<std::uint32_t>(anchors.size());
                result.focus.actionIndex = IsUp(a_keyCode) ?
                    (result.focus.actionIndex + count - 1) % count :
                    (result.focus.actionIndex + 1) % count;
            } else if (IsAccept(a_keyCode)) {
                result.command = AnchorCommand(
                    page, anchors[result.focus.actionIndex]);
                result.focus.region = FocusRegion::Controls;
            }
            return result;
        }

        if (result.focus.region == FocusRegion::Grid) {
            if (!grid) {
                result.focus.region = FocusRegion::Controls;
                return result;
            }
            if (IsUp(a_keyCode) || IsDown(a_keyCode)) {
                const auto count = static_cast<std::size_t>(
                    grid->descriptor.segmentedGrid.columnCount);
                if (count != 0) {
                    const auto selected = SelectedGridColumnIndex(a_model, *grid);
                    const auto target = IsUp(a_keyCode) ?
                        (selected + count - 1) % count : (selected + 1) % count;
                    result.command = GridSelectionCommand(page, *grid, target);
                }
            } else if (IsAccept(a_keyCode)) {
                result.focus.region = FocusRegion::Controls;
            }
            return result;
        }

        if (result.focus.region == FocusRegion::Modules) {
            if ((IsUp(a_keyCode) || IsDown(a_keyCode)) &&
                !a_model.modules.empty()) {
                const auto moduleCount = a_model.modules.size();
                const auto modulePosition = ActiveModuleIndex(a_model);
                const auto target = IsUp(a_keyCode) ?
                    (modulePosition + moduleCount - 1) % moduleCount :
                    (modulePosition + 1) % moduleCount;
                result.command = ModuleCommand(a_model.modules[target]);
            }
            return result;
        }

        if (result.focus.region == FocusRegion::Actions) {
            if (IsUp(a_keyCode) || IsDown(a_keyCode)) {
                result.focus.actionIndex = IsUp(a_keyCode) ?
                    (result.focus.actionIndex + kActionCount - 1) % kActionCount :
                    (result.focus.actionIndex + 1) % kActionCount;
            } else if (IsAccept(a_keyCode)) {
                result.command = ActionCommand(page, result.focus.actionIndex);
            }
            return result;
        }

        if (page.controls.empty()) {
            return result;
        }
        const auto selected = (std::min)(SelectedControlIndex(a_model, page),
            page.controls.size() - 1);
        if (IsUp(a_keyCode) || IsDown(a_keyCode)) {
            const auto count = page.controls.size();
            auto target = selected;
            do {
                target = IsUp(a_keyCode) ?
                    (target + count - 1) % count : (target + 1) % count;
            } while (target != selected && !Selectable(page.controls[target]));
            if (!Selectable(page.controls[target])) return result;
            result.command = ControlCommand(
                MenuSession::CommandKind::SelectControl, page, page.controls[target]);
            return result;
        }
        if (a_keyCode == kDecrease || a_keyCode == kIncrease) {
            result.command = Adjust(page, page.controls[selected],
                a_keyCode == kDecrease ? -1 : 1);
            return result;
        }
        if (IsAccept(a_keyCode)) {
            result.command = Activate(page, page.controls[selected]);
        }
        return result;
    }

}
