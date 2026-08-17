#include "MenuInputRouter.h"

#include <algorithm>
#include <cmath>
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
            return found == a_page.controls.end() ? 0 :
                static_cast<std::size_t>(std::distance(a_page.controls.begin(), found));
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
        result.focus.actionIndex = (std::min)(result.focus.actionIndex, kActionCount - 1);
        if (!result.handled) {
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
            if (IsRight(a_keyCode) && result.focus.region == FocusRegion::Modules) {
                result.focus.region = FocusRegion::Controls;
            } else if (IsLeft(a_keyCode) && result.focus.region == FocusRegion::Controls) {
                result.focus.region = FocusRegion::Modules;
            } else if (IsRight(a_keyCode) && result.focus.region == FocusRegion::Controls) {
                result.focus.region = FocusRegion::Actions;
                result.focus.actionIndex = 0;
            } else if (IsLeft(a_keyCode) && result.focus.region == FocusRegion::Actions) {
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
            const auto target = IsUp(a_keyCode) ?
                (selected + count - 1) % count : (selected + 1) % count;
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
