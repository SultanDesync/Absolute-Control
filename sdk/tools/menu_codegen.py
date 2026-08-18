#!/usr/bin/env python3
"""Validate ACP menu JSON and generate deterministic ABI-v1 C++ descriptors."""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import re
import sys
from typing import Any


ID_RE = re.compile(r"^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$")
ROOT_REQUIRED = {"schemaVersion", "targetAbiVersion", "module", "pages"}
ROOT_KEYS = ROOT_REQUIRED | {"$schema"}
MODULE_KEYS = {"id", "title", "description"}
PAGE_KEYS = {"id", "title", "description", "sections"}
SECTION_KEYS = {"id", "title", "description", "options"}
COMMON_KEYS = {"id", "type", "label", "description", "flags"}
TYPE_KEYS = {
    "toggle": COMMON_KEYS,
    "integer": COMMON_KEYS | {"widget", "minimum", "maximum", "step"},
    "float": COMMON_KEYS | {"widget", "minimum", "maximum", "step"},
    "action": COMMON_KEYS,
    "inputBinding": COMMON_KEYS | {"capabilities"},
}
FLAGS = {"readOnly", "requiresRestart", "advanced", "layoutInline"}
CAPABILITIES = {"keyboard", "mouse", "controller", "modifiers", "clearable"}
KINDS = {
    "toggle": "Toggle",
    "integer": "IntegerSlider",
    "float": "FloatSlider",
    "action": "Action",
    "inputBinding": "InputBinding",
}


class ValidationError(Exception):
    pass


def fail(path: str, message: str) -> None:
    raise ValidationError(f"{path}: {message}")


def object_at(value: Any, path: str, allowed: set[str], required: set[str]) -> dict[str, Any]:
    if not isinstance(value, dict):
        fail(path, "must be an object")
    unknown = sorted(set(value) - allowed)
    if unknown:
        fail(path, f"unknown properties: {', '.join(unknown)}")
    missing = sorted(required - set(value))
    if missing:
        fail(path, f"missing required properties: {', '.join(missing)}")
    return value


def text_at(value: Any, path: str, capacity: int, *, identifier: bool = False, empty: bool = False) -> str:
    if not isinstance(value, str) or (not empty and not value):
        fail(path, "must be a non-empty string" if not empty else "must be a string")
    if len(value.encode("utf-8")) >= capacity:
        fail(path, f"must fit in {capacity - 1} UTF-8 bytes for ABI v1")
    if identifier and not ID_RE.fullmatch(value):
        fail(path, "must be a lowercase stable identifier")
    return value


def array_at(value: Any, path: str, minimum: int, maximum: int) -> list[Any]:
    if not isinstance(value, list) or not minimum <= len(value) <= maximum:
        fail(path, f"must contain {minimum}..{maximum} items")
    return value


def validate_option(raw: Any, path: str, seen_controls: set[str]) -> dict[str, Any]:
    if not isinstance(raw, dict):
        fail(path, "must be an object")
    option_type = raw.get("type")
    if option_type not in TYPE_KEYS:
        fail(f"{path}.type", f"unsupported by ABI v1: {option_type!r}")
    required = {"id", "type", "label"}
    if option_type in {"integer", "float"}:
        required |= {"widget", "minimum", "maximum", "step"}
    if option_type == "inputBinding":
        required.add("capabilities")
    option = object_at(raw, path, TYPE_KEYS[option_type], required)
    control_id = text_at(option["id"], f"{path}.id", 64, identifier=True)
    if control_id in seen_controls:
        fail(f"{path}.id", "control IDs must be unique across the module because ABI v1 callbacks receive only controlId")
    seen_controls.add(control_id)
    text_at(option["label"], f"{path}.label", 96)
    text_at(option.get("description", ""), f"{path}.description", 192, empty=True)
    flags = option.get("flags", [])
    if not isinstance(flags, list) or any(flag not in FLAGS for flag in flags) or len(flags) != len(set(flags)):
        fail(f"{path}.flags", "must contain unique ABI-v1 flag names")
    if option_type == "action" and "readOnly" in flags:
        fail(f"{path}.flags", "readOnly is not meaningful for an action")
    if "layoutInline" in flags and option_type != "action":
        fail(f"{path}.flags", "layoutInline is supported only for actions")
    if option_type in {"integer", "float"}:
        if option["widget"] != "slider":
            fail(f"{path}.widget", "ABI v1 supports only slider")
        numeric_type = int if option_type == "integer" else (int, float)
        for name in ("minimum", "maximum", "step"):
            value = option[name]
            if isinstance(value, bool) or not isinstance(value, numeric_type) or not math.isfinite(value):
                fail(f"{path}.{name}", f"must be a finite {option_type}")
        if option["minimum"] >= option["maximum"]:
            fail(path, "minimum must be less than maximum")
        if option["step"] <= 0 or option["step"] > option["maximum"] - option["minimum"]:
            fail(f"{path}.step", "must be positive and no larger than the range")
    if option_type == "inputBinding":
        caps = object_at(option["capabilities"], f"{path}.capabilities", CAPABILITIES, set())
        if not caps or any(not isinstance(value, bool) for value in caps.values()):
            fail(f"{path}.capabilities", "must contain Boolean capability values")
        if not any(caps.get(name, False) for name in ("keyboard", "mouse", "controller")):
            fail(f"{path}.capabilities", "must enable at least one input device")
        if caps.get("modifiers") and not caps.get("keyboard"):
            fail(f"{path}.capabilities.modifiers", "requires keyboard=true")
    return option


def validate(data: Any) -> dict[str, Any]:
    root = object_at(data, "$", ROOT_KEYS, ROOT_REQUIRED)
    if root["schemaVersion"] != 1:
        fail("$.schemaVersion", "must be 1")
    if root["targetAbiVersion"] != 1:
        fail("$.targetAbiVersion", "only ABI v1 is supported")
    module = object_at(root["module"], "$.module", MODULE_KEYS, {"id", "title"})
    text_at(module["id"], "$.module.id", 64, identifier=True)
    text_at(module["title"], "$.module.title", 96)
    text_at(module.get("description", ""), "$.module.description", 192, empty=True)
    pages = array_at(root["pages"], "$.pages", 1, 32)
    seen_pages: set[str] = set()
    seen_page_symbols: set[str] = set()
    seen_controls: set[str] = set()
    seen_control_symbols: set[str] = set()
    total_control_count = 0
    for pi, raw_page in enumerate(pages):
        path = f"$.pages[{pi}]"
        page = object_at(raw_page, path, PAGE_KEYS, {"id", "title", "sections"})
        page_id = text_at(page["id"], f"{path}.id", 64, identifier=True)
        if page_id in seen_pages:
            fail(f"{path}.id", "duplicate page ID")
        seen_pages.add(page_id)
        page_symbol = cpp_name(page_id)
        if page_symbol in seen_page_symbols:
            fail(f"{path}.id", "collides with another page ID after C++ name normalization")
        seen_page_symbols.add(page_symbol)
        text_at(page["title"], f"{path}.title", 96)
        text_at(page.get("description", ""), f"{path}.description", 192, empty=True)
        sections = array_at(page["sections"], f"{path}.sections", 1, 32)
        seen_sections: set[str] = set()
        control_count = 0
        for si, raw_section in enumerate(sections):
            section_path = f"{path}.sections[{si}]"
            section = object_at(raw_section, section_path, SECTION_KEYS, {"id", "title", "options"})
            section_id = text_at(section["id"], f"{section_path}.id", 64, identifier=True)
            if section_id in seen_sections:
                fail(f"{section_path}.id", "duplicate section ID within page")
            seen_sections.add(section_id)
            text_at(section["title"], f"{section_path}.title", 96)
            text_at(section.get("description", ""),
                    f"{section_path}.description", 192, empty=True)
            options = array_at(section["options"], f"{section_path}.options", 1, 128)
            # Every section emits one presentation-only GroupHeader control.
            control_count += len(options) + 1
            total_control_count += len(options) + 1
            if control_count > 128:
                fail(f"{path}.sections", "page exceeds the ABI-v1 limit of 128 controls")
            if total_control_count > 512:
                fail("$.pages", "module exceeds the ABI-v1 limit of 512 controls")
            for oi, option in enumerate(options):
                option_path = f"{section_path}.options[{oi}]"
                validated = validate_option(option, option_path, seen_controls)
                control_symbol = cpp_name(validated["id"])
                if control_symbol in seen_control_symbols:
                    fail(f"{option_path}.id", "collides with another control ID after C++ name normalization")
                seen_control_symbols.add(control_symbol)
    return root


def cpp_string(value: str) -> str:
    result = '"'
    for byte in value.encode("utf-8"):
        if byte == 0x22:
            result += r'\"'
        elif byte == 0x5C:
            result += r'\\'
        elif 0x20 <= byte <= 0x7E:
            result += chr(byte)
        else:
            result += f"\\{byte:03o}"
    return result + '"'


def cpp_number(value: int | float) -> str:
    if isinstance(value, int):
        return f"{value}.0"
    return format(value, ".17g")


def cpp_name(value: str) -> str:
    return "_".join(part for part in re.split(r"[^a-zA-Z0-9]+", value) if part)


def flag_expression(option: dict[str, Any], structured_layout: bool = True) -> str:
    mapping = {
        "readOnly": "kControlReadOnly",
        "requiresRestart": "kControlRequiresRestart",
        "advanced": "kControlAdvanced",
        "layoutInline": "kControlLayoutInline",
    }
    values = [mapping[name] for name in option.get("flags", [])
              if structured_layout or name != "layoutInline"]
    if option["type"] == "inputBinding":
        cap_mapping = {
            "keyboard": "kBindingKeyboard",
            "mouse": "kBindingMouse",
            "controller": "kBindingController",
            "modifiers": "kBindingModifiers",
            "clearable": "kBindingClearable",
        }
        values += [flag for name, flag in cap_mapping.items() if option["capabilities"].get(name, False)]
    return " | ".join(values) if values else "kControlNone"


def generate(data: dict[str, Any], source_name: str) -> str:
    module = data["module"]
    namespace = "AbsoluteControlPanelGenerated::module_" + cpp_name(module["id"])
    lines = [
        "// Generated by sdk/tools/menu_codegen.py; do not edit.",
        f"// Source: {source_name}",
        "#pragma once",
        "",
        "#include <AbsoluteControlPanelAPI.h>",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        f"namespace {namespace}",
        "{",
        "    using namespace AbsoluteControlPanelApi;",
        "",
        "    template <std::size_t N, std::size_t M>",
        "    constexpr void CopyLiteral(char (&destination)[N], const char (&source)[M])",
        "    {",
        "        static_assert(M <= N);",
        "        for (std::size_t index = 0; index < M; ++index) destination[index] = source[index];",
        "    }",
        "",
        "    template <std::size_t I, std::size_t L, std::size_t D>",
        "    consteval ControlDescriptorV1 Control(ControlKind kind, std::uint32_t flags,",
        "        const char (&id)[I], const char (&label)[L], const char (&description)[D],",
        "        double minimum = 0.0, double maximum = 0.0, double step = 0.0)",
        "    {",
        "        ControlDescriptorV1 value{};",
        "        value.kind = kind; value.flags = flags;",
        "        CopyLiteral(value.controlId, id); CopyLiteral(value.label, label);",
        "        CopyLiteral(value.description, description);",
        "        value.minimumValue = minimum; value.maximumValue = maximum; value.stepValue = step;",
        "        return value;",
        "    }",
        "",
        "    inline constexpr ModuleDescriptorV1 kModule = []() consteval {",
        "        ModuleDescriptorV1 value{};",
        f"        CopyLiteral(value.moduleId, {cpp_string(module['id'])});",
        f"        CopyLiteral(value.displayName, {cpp_string(module['title'])});",
        f"        CopyLiteral(value.description, {cpp_string(module.get('description', ''))});",
        "        return value;",
        "    }();",
        "",
        "    enum class ControlId : std::uint32_t",
        "    {",
    ]
    all_options: list[dict[str, Any]] = []
    for page in data["pages"]:
        for section in page["sections"]:
            all_options.extend(section["options"])
    for option in all_options:
        lines.append(f"        id_{cpp_name(option['id'])},")
    lines += ["        Unknown", "    };", "", "    constexpr ControlId ParseControlId(std::string_view id) noexcept", "    {"]
    for option in all_options:
        lines.append(f"        if (id == {cpp_string(option['id'])}) return ControlId::id_{cpp_name(option['id'])};")
    lines += ["        return ControlId::Unknown;", "    }", ""]
    for page in data["pages"]:
        base_name = "k" + cpp_name(page["id"]).title().replace("_", "")
        for structured_layout in (True, False):
            control_count = sum(
                len(section["options"]) + (1 if structured_layout else 0)
                for section in page["sections"]
            )
            array_name = base_name + ("Controls" if structured_layout
                                      else "LegacyControls")
            lines.append(f"    inline constexpr std::array<ControlDescriptorV1, {control_count}> {array_name}{{{{")
            for section_index, section in enumerate(page["sections"]):
                if structured_layout:
                    lines.append(
                        f"        Control(ControlKind::GroupHeader, kControlNone, "
                        f"{cpp_string('__section_' + str(section_index))}, "
                        f"{cpp_string(section['title'])}, "
                        f"{cpp_string(section.get('description', ''))}),"
                    )
                for option in section["options"]:
                    minimum = option.get("minimum", 0)
                    maximum = option.get("maximum", 0)
                    step = option.get("step", 0)
                    lines.append(
                        f"        Control(ControlKind::{KINDS[option['type']]}, "
                        f"{flag_expression(option, structured_layout)}, "
                        f"{cpp_string(option['id'])}, {cpp_string(option['label'])}, "
                        f"{cpp_string(option.get('description', ''))}, {cpp_number(minimum)}, "
                        f"{cpp_number(maximum)}, {cpp_number(step)}),"
                    )
            lines += ["    }};", ""]
    lines += [
        "    struct ProviderCallbacks",
        "    {",
        "        void* context{};",
        "        ReadValueCallback readValue{};",
        "        WriteDraftCallback writeDraft{};",
        "        InvokeActionCallback invokeAction{};",
        "        ApplyCallback apply{};",
        "        CancelCallback cancel{};",
        "        ReadChoiceOptionsCallback readChoiceOptions{};",
        "        BeginBindingCaptureCallback beginBindingCapture{};",
        "        PollBindingCaptureCallback pollBindingCapture{};",
        "        CancelBindingCaptureCallback cancelBindingCapture{};",
        "        ReassignBindingCallback reassignBinding{};",
        "    };",
        "",
        f"    inline std::array<PageDescriptorV1, {len(data['pages'])}> MakePages(",
        "        const ProviderCallbacks& provider,",
        "        std::uint64_t hostCapabilities) noexcept",
        "    {",
        f"        std::array<PageDescriptorV1, {len(data['pages'])}> pages{{}};",
        "        const bool structuredLayout =",
        "            (hostCapabilities & kCapabilityStructuredLayout) != 0;",
    ]
    for index, page in enumerate(data["pages"]):
        base_name = "k" + cpp_name(page["id"]).title().replace("_", "")
        array_name = base_name + "Controls"
        legacy_array_name = base_name + "LegacyControls"
        lines += [
            f"        CopyLiteral(pages[{index}].moduleId, {cpp_string(module['id'])});",
            f"        CopyLiteral(pages[{index}].pageId, {cpp_string(page['id'])});",
            f"        CopyLiteral(pages[{index}].displayName, {cpp_string(page['title'])});",
            f"        CopyLiteral(pages[{index}].description, {cpp_string(page.get('description', ''))});",
            f"        pages[{index}].controlCount = static_cast<std::uint32_t>(structuredLayout ?",
            f"            {array_name}.size() : {legacy_array_name}.size());",
            f"        pages[{index}].controls = structuredLayout ?",
            f"            {array_name}.data() : {legacy_array_name}.data();",
            f"        pages[{index}].context = provider.context;",
            f"        pages[{index}].readValue = provider.readValue;",
            f"        pages[{index}].writeDraft = provider.writeDraft;",
            f"        pages[{index}].invokeAction = provider.invokeAction;",
            f"        pages[{index}].apply = provider.apply;",
            f"        pages[{index}].cancel = provider.cancel;",
            f"        pages[{index}].readChoiceOptions = provider.readChoiceOptions;",
            f"        pages[{index}].beginBindingCapture = provider.beginBindingCapture;",
            f"        pages[{index}].pollBindingCapture = provider.pollBindingCapture;",
            f"        pages[{index}].cancelBindingCapture = provider.cancelBindingCapture;",
            f"        pages[{index}].reassignBinding = provider.reassignBinding;",
        ]
    lines += ["        return pages;", "    }", "}", ""]
    return "\n".join(lines)


def load(path: pathlib.Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationError(str(error)) from error
    return validate(data)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("definition", type=pathlib.Path)
    generate_parser = subparsers.add_parser("generate")
    generate_parser.add_argument("definition", type=pathlib.Path)
    generate_parser.add_argument("output", type=pathlib.Path)
    generate_parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    try:
        data = load(args.definition)
        if args.command == "validate":
            print(f"validated {args.definition}")
            return 0
        rendered = generate(data, args.definition.name)
        if args.check:
            if not args.output.exists() or args.output.read_text(encoding="utf-8") != rendered:
                print(f"generated output is stale: {args.output}", file=sys.stderr)
                return 1
            print(f"generated output is current: {args.output}")
            return 0
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8", newline="\n")
        print(f"generated {args.output}")
        return 0
    except ValidationError as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
