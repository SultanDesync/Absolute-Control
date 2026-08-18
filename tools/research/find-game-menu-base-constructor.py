#!/usr/bin/env python3
"""Recover GameMenuBase constructor candidates from a Starfield executable.

The 1.16.244 compiler-breaking update invalidated historical CommonLibSF
function IDs.  This tool uses current Address Library v2 vtable mappings,
PE exception-function ranges, and RIP-relative vtable references to locate
the base constructor without executing unverified code in Starfield.
"""

from __future__ import annotations

import argparse
import bisect
import collections
import json
import pathlib
import re
import struct
import sys
from dataclasses import dataclass


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
LOCAL_PACKAGES = REPOSITORY_ROOT / ".tools" / "py"
if LOCAL_PACKAGES.is_dir():
    sys.path.insert(0, str(LOCAL_PACKAGES))

import capstone  # type: ignore[import-not-found]  # noqa: E402
from capstone.x86_const import (  # noqa: E402
    X86_OP_IMM,
    X86_OP_MEM,
    X86_REG_RIP,
)
import pefile  # type: ignore[import-not-found]  # noqa: E402


GAME_MENU_BASE_VTABLE_IDS = (434723, 434725, 434721)
DERIVED_PRIMARY_VTABLE_IDS = {
    "ContainerMenu": 460157,
    "DataMenu": 445393,
    "HUDMenu": 460481,
    "InventoryMenu": 1036602,
    "LoadingMenu": 460518,
    "MessageBoxMenu": 460795,
    "PauseMenu": 445675,
}


@dataclass(frozen=True)
class FunctionRange:
    begin: int
    end: int


def read_exact(stream, count: int) -> bytes:
    value = stream.read(count)
    if len(value) != count:
        raise ValueError("Address Library ended unexpectedly")
    return value


def read_integer(stream, byte_count: int) -> int:
    return int.from_bytes(read_exact(stream, byte_count), "little", signed=False)


def load_address_library(path: pathlib.Path) -> dict[int, int]:
    with path.open("rb") as stream:
        file_format = read_integer(stream, 4)
        if file_format not in (1, 2, 5):
            raise ValueError(
                f"Expected Address Library v1/v2/v5 format, found {file_format}"
            )

        version = struct.unpack("<4I", read_exact(stream, 16))
        if file_format == 5:
            name = read_exact(stream, 64).split(b"\0", 1)[0].decode(
                "utf-8", errors="replace"
            )
            pointer_size = read_integer(stream, 4)
            data_format = read_integer(stream, 4)
            offset_count = read_integer(stream, 4)
            mappings = {
                identifier: offset
                for identifier, (offset,) in enumerate(
                    struct.iter_unpack("<I", read_exact(stream, offset_count * 4))
                )
                if offset
            }
            print(
                f"Address Library: format={file_format} "
                f"version={'.'.join(map(str, version))} name={name!r} "
                f"pointer_size={pointer_size} data_format={data_format} "
                f"offsets={offset_count} populated={len(mappings)}"
            )
            return mappings

        name_length = read_integer(stream, 4)
        name = read_exact(stream, name_length).decode("utf-8", errors="replace")
        pointer_size = read_integer(stream, 4)
        address_count = read_integer(stream, 4)

        mappings: dict[int, int] = {}
        previous_id = 0
        previous_offset = 0
        for _ in range(address_count):
            entry_type = read_integer(stream, 1)
            id_encoding = entry_type & 0x0F
            offset_encoding = entry_type >> 4

            if id_encoding == 0:
                identifier = read_integer(stream, 8)
            elif id_encoding == 1:
                identifier = previous_id + 1
            elif id_encoding == 2:
                identifier = previous_id + read_integer(stream, 1)
            elif id_encoding == 3:
                identifier = previous_id - read_integer(stream, 1)
            elif id_encoding == 4:
                identifier = previous_id + read_integer(stream, 2)
            elif id_encoding == 5:
                identifier = previous_id - read_integer(stream, 2)
            elif id_encoding == 6:
                identifier = read_integer(stream, 2)
            elif id_encoding == 7:
                identifier = read_integer(stream, 4)
            else:
                raise ValueError(f"Unsupported ID encoding {id_encoding}")

            scaled = (offset_encoding & 8) != 0
            previous_unit = previous_offset // pointer_size if scaled else previous_offset
            offset_kind = offset_encoding & 7
            if offset_kind == 0:
                offset = read_integer(stream, 8)
            elif offset_kind == 1:
                offset = previous_unit + 1
            elif offset_kind == 2:
                offset = previous_unit + read_integer(stream, 1)
            elif offset_kind == 3:
                offset = previous_unit - read_integer(stream, 1)
            elif offset_kind == 4:
                offset = previous_unit + read_integer(stream, 2)
            elif offset_kind == 5:
                offset = previous_unit - read_integer(stream, 2)
            elif offset_kind == 6:
                offset = read_integer(stream, 2)
            elif offset_kind == 7:
                offset = read_integer(stream, 4)
            else:
                raise AssertionError("unreachable")
            if scaled:
                offset *= pointer_size

            mappings[identifier] = offset
            previous_id = identifier
            previous_offset = offset

    print(
        f"Address Library: format={file_format} version={'.'.join(map(str, version))} "
        f"name={name!r} mappings={len(mappings)}"
    )
    return mappings


def make_disassembler() -> capstone.Cs:
    disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    disassembler.detail = True
    disassembler.skipdata = True
    return disassembler


RIP_RELATIVE_LOAD = re.compile(
    rb"[\x48-\x4f](?:\x8d|\x8b)[\x05\x0d\x15\x1d\x25\x2d\x35\x3d].{4}",
    re.DOTALL,
)


def scan_rip_relative_xrefs(
    code: bytes, code_rva: int, targets: set[int]
) -> dict[int, list[int]]:
    """Find 64-bit RIP-relative LEA/MOV references without linear disassembly."""
    xrefs: dict[int, list[int]] = collections.defaultdict(list)
    for match in RIP_RELATIVE_LOAD.finditer(code):
        displacement = struct.unpack_from("<i", match.group(), 3)[0]
        instruction_rva = code_rva + match.start()
        target = instruction_rva + 7 + displacement
        if target in targets:
            xrefs[target].append(instruction_rva)
    return xrefs


def scan_direct_call_xrefs(
    code: bytes, code_rva: int, targets: set[int]
) -> dict[int, list[int]]:
    """Find direct near-call sites whose resolved RVA matches a target."""
    xrefs: dict[int, list[int]] = collections.defaultdict(list)
    for index in range(len(code) - 4):
        if code[index] != 0xE8:
            continue
        site = code_rva + index
        displacement = struct.unpack_from("<i", code, index + 1)[0]
        target = site + 5 + displacement
        if target in targets:
            xrefs[target].append(site)
    return xrefs


def rip_targets(instruction, image_base: int) -> list[int]:
    if instruction.id == 0:
        return []
    targets: list[int] = []
    for operand in instruction.operands:
        if operand.type == X86_OP_MEM and operand.mem.base == X86_REG_RIP:
            target_va = instruction.address + instruction.size + operand.mem.disp
            targets.append(target_va - image_base)
    return targets


def direct_call_target(instruction, image_base: int) -> int | None:
    if instruction.id == 0 or instruction.mnemonic != "call" or not instruction.operands:
        return None
    operand = instruction.operands[0]
    if operand.type != X86_OP_IMM:
        return None
    return int(operand.imm) - image_base


def function_ranges(pe: pefile.PE) -> list[FunctionRange]:
    entries = getattr(pe, "DIRECTORY_ENTRY_EXCEPTION", ())
    ranges = [
        FunctionRange(int(entry.struct.BeginAddress), int(entry.struct.EndAddress))
        for entry in entries
        if int(entry.struct.EndAddress) > int(entry.struct.BeginAddress)
    ]
    ranges.sort(key=lambda value: value.begin)
    return ranges


def containing_function(
    ranges: list[FunctionRange], begins: list[int], address: int
) -> FunctionRange | None:
    index = bisect.bisect_right(begins, address) - 1
    if index < 0:
        return None
    candidate = ranges[index]
    return candidate if address < candidate.end else None


def function_instructions(
    pe: pefile.PE,
    disassembler: capstone.Cs,
    function: FunctionRange,
    image_base: int,
):
    code = pe.get_data(function.begin, function.end - function.begin)
    return list(disassembler.disasm(code, image_base + function.begin))


def format_instruction(instruction, image_base: int) -> str:
    return (
        f"{instruction.address - image_base:08X}  "
        f"{instruction.mnemonic:<8} {instruction.op_str}"
    ).rstrip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True, type=pathlib.Path)
    parser.add_argument("--address-library", required=True, type=pathlib.Path)
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=REPOSITORY_ROOT / "artifacts" / "game-menu-base-candidates.json",
    )
    parser.add_argument("--top", type=int, default=20)
    parser.add_argument(
        "--inspect-id",
        action="append",
        type=int,
        default=[],
        help="also disassemble the function containing this Address Library ID",
    )
    parser.add_argument(
        "--inspect-rva",
        action="append",
        type=lambda value: int(value, 0),
        default=[],
        help="also disassemble the function containing this image-relative address",
    )
    parser.add_argument(
        "--xref-rva",
        action="append",
        type=lambda value: int(value, 0),
        default=[],
        help="report direct call sites to this image-relative address",
    )
    parser.add_argument(
        "--inspect-vtable-id",
        action="append",
        type=int,
        default=[],
        help="decode function entries at this Address Library vtable ID",
    )
    arguments = parser.parse_args()

    mappings = load_address_library(arguments.address_library)
    offset_to_ids: dict[int, list[int]] = collections.defaultdict(list)
    for identifier, offset in mappings.items():
        if offset:
            offset_to_ids[offset].append(identifier)

    pe = pefile.PE(str(arguments.exe), fast_load=False)
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    ranges = function_ranges(pe)
    begins = [value.begin for value in ranges]
    print(f"PE image_base=0x{image_base:X} exception_functions={len(ranges)}")

    text_section = next(
        section for section in pe.sections if section.Name.rstrip(b"\0") == b".text"
    )
    text_rva = int(text_section.VirtualAddress)
    text = text_section.get_data()
    disassembler = make_disassembler()

    base_vtables = {mappings[value] for value in GAME_MENU_BASE_VTABLE_IDS}
    derived_vtables = {
        name: mappings[identifier]
        for name, identifier in DERIVED_PRIMARY_VTABLE_IDS.items()
        if mappings.get(identifier)
    }
    targets = base_vtables | set(derived_vtables.values())
    xrefs = scan_rip_relative_xrefs(text, text_rva, targets)

    base_functions: dict[int, dict[str, object]] = {}
    for vtable_rva in sorted(base_vtables):
        for xref in xrefs[vtable_rva]:
            function = containing_function(ranges, begins, xref)
            if function is None:
                continue
            record = base_functions.setdefault(
                function.begin,
                {"begin": function.begin, "end": function.end, "references": []},
            )
            record["references"].append({"instruction": xref, "vtable": vtable_rva})

    derived_function_records: list[dict[str, object]] = []
    common_pre_vtable_calls: dict[int, set[str]] = collections.defaultdict(set)
    call_sites: dict[int, list[dict[str, object]]] = collections.defaultdict(list)
    for menu_name, vtable_rva in derived_vtables.items():
        for xref in xrefs[vtable_rva]:
            function = containing_function(ranges, begins, xref)
            if function is None:
                continue
            instructions = function_instructions(pe, disassembler, function, image_base)
            calls_before_reference = []
            calls_after_reference = []
            for instruction in instructions:
                instruction_rva = int(instruction.address - image_base)
                target = direct_call_target(instruction, image_base)
                if target is None:
                    continue
                call = {
                    "site": instruction_rva,
                    "target": target,
                    "address_library_ids": offset_to_ids.get(target, []),
                }
                if instruction_rva >= xref:
                    calls_after_reference.append(call)
                    continue
                calls_before_reference.append(call)
                target_function = containing_function(ranges, begins, target)
                normalized_target = target_function.begin if target_function else target
                common_pre_vtable_calls[normalized_target].add(menu_name)
                call_sites[normalized_target].append(
                    {"menu": menu_name, "site": instruction_rva, "xref": xref}
                )
            derived_function_records.append(
                {
                    "menu": menu_name,
                    "begin": function.begin,
                    "end": function.end,
                    "vtable_reference": xref,
                    "address_library_ids": offset_to_ids.get(function.begin, []),
                    "calls_before_reference": calls_before_reference,
                    "calls_after_reference": calls_after_reference,
                    "disassembly": [
                        format_instruction(instruction, image_base)
                        for instruction in instructions[:240]
                    ],
                }
            )

    ranked_calls = sorted(
        common_pre_vtable_calls,
        key=lambda target: (
            -len(common_pre_vtable_calls[target]),
            -len(call_sites[target]),
            target,
        ),
    )

    derived_constructors = {
        int(record["begin"]): str(record["menu"])
        for record in derived_function_records
        if any(
            int(call["target"]) == mappings[130615]
            for call in record["calls_before_reference"]
        )
    }
    constructor_call_xrefs = scan_direct_call_xrefs(
        text, text_rva, set(derived_constructors)
    )
    derived_factory_functions = []
    seen_factory_ranges: set[tuple[str, int, int]] = set()
    for constructor, menu_name in sorted(derived_constructors.items()):
        for site in constructor_call_xrefs.get(constructor, []):
            function = containing_function(ranges, begins, site)
            if function is None:
                continue
            key = (menu_name, function.begin, function.end)
            if key in seen_factory_ranges:
                continue
            seen_factory_ranges.add(key)
            instructions = function_instructions(pe, disassembler, function, image_base)
            derived_factory_functions.append(
                {
                    "menu": menu_name,
                    "constructor": constructor,
                    "constructor_call_site": site,
                    "begin": function.begin,
                    "end": function.end,
                    "address_library_ids": offset_to_ids.get(function.begin, []),
                    "disassembly": [
                        format_instruction(instruction, image_base)
                        for instruction in instructions[:240]
                    ],
                }
            )
    candidate_details = []
    for target in ranked_calls[: arguments.top]:
        function = containing_function(ranges, begins, target)
        if function is None:
            function = FunctionRange(target, target + 32)
        instructions = function_instructions(pe, disassembler, function, image_base)
        candidate_details.append(
            {
                "begin": function.begin,
                "end": function.end,
                "address_library_ids": offset_to_ids.get(function.begin, []),
                "distinct_menus": sorted(common_pre_vtable_calls[target]),
                "call_sites": call_sites[target],
                "disassembly": [
                    format_instruction(instruction, image_base)
                    for instruction in instructions[:160]
                ],
            }
        )

    triple_base_functions = []
    for begin, record in sorted(base_functions.items()):
        referenced = {item["vtable"] for item in record["references"]}
        if referenced != base_vtables:
            continue
        function = FunctionRange(int(record["begin"]), int(record["end"]))
        instructions = function_instructions(pe, disassembler, function, image_base)
        triple_base_functions.append(
            {
                **record,
                "address_library_ids": offset_to_ids.get(begin, []),
                "disassembly": [
                    format_instruction(instruction, image_base)
                    for instruction in instructions[:200]
                ],
            }
        )

    vtable_entries: dict[str, list[dict[str, object]]] = {}
    for vtable_rva in sorted(base_vtables):
        entries = []
        file_offset = pe.get_offset_from_rva(vtable_rva)
        raw = pe.__data__[file_offset : file_offset + (8 * 32)]
        for index, (address,) in enumerate(struct.iter_unpack("<Q", raw)):
            function_rva = address - image_base if address >= image_base else address
            entries.append(
                {
                    "index": index,
                    "rva": function_rva,
                    "address_library_ids": offset_to_ids.get(function_rva, []),
                }
            )
        vtable_entries[f"0x{vtable_rva:08X}"] = entries

    derived_vtable_entries: dict[str, list[dict[str, object]]] = {}
    for menu_name, vtable_rva in sorted(derived_vtables.items()):
        entries = []
        file_offset = pe.get_offset_from_rva(vtable_rva)
        raw = pe.__data__[file_offset : file_offset + (8 * 32)]
        for index, (address,) in enumerate(struct.iter_unpack("<Q", raw)):
            function_rva = address - image_base if address >= image_base else address
            entries.append(
                {
                    "index": index,
                    "rva": function_rva,
                    "address_library_ids": offset_to_ids.get(function_rva, []),
                }
            )
        derived_vtable_entries[menu_name] = entries

    inspected_vtables: dict[str, list[dict[str, object]]] = {}
    for identifier in arguments.inspect_vtable_id:
        vtable_rva = mappings.get(identifier)
        if vtable_rva is None:
            inspected_vtables[str(identifier)] = []
            continue
        entries = []
        file_offset = pe.get_offset_from_rva(vtable_rva)
        raw = pe.__data__[file_offset : file_offset + (8 * 16)]
        for index, (address,) in enumerate(struct.iter_unpack("<Q", raw)):
            function_rva = address - image_base if address >= image_base else address
            entries.append(
                {
                    "index": index,
                    "rva": function_rva,
                    "address_library_ids": offset_to_ids.get(function_rva, []),
                }
            )
        inspected_vtables[str(identifier)] = entries

    inspected_functions = []
    for identifier in arguments.inspect_id:
        target = mappings.get(identifier)
        if target is None:
            inspected_functions.append(
                {"address_library_id": identifier, "mapped": False}
            )
            continue
        function = containing_function(ranges, begins, target)
        if function is None:
            function = FunctionRange(target, target + 32)
        instructions = function_instructions(pe, disassembler, function, image_base)
        inspected_functions.append(
            {
                "address_library_id": identifier,
                "mapped": True,
                "target": target,
                "begin": function.begin,
                "end": function.end,
                "disassembly": [
                    format_instruction(instruction, image_base)
                    for instruction in instructions[:240]
                ],
            }
        )
    inspected_call_xrefs = []
    for target in arguments.xref_rva:
        sites = scan_direct_call_xrefs(text, text_rva, {target}).get(target, [])
        for site in sites:
            function = containing_function(ranges, begins, site)
            if function is None:
                continue
            instructions = function_instructions(pe, disassembler, function, image_base)
            call_index = next(
                (
                    index
                    for index, instruction in enumerate(instructions)
                    if int(instruction.address - image_base) == site
                ),
                0,
            )
            first = max(0, call_index - 16)
            last = min(len(instructions), call_index + 5)
            inspected_call_xrefs.append(
                {
                    "target": target,
                    "site": site,
                    "function_begin": function.begin,
                    "function_end": function.end,
                    "address_library_ids": offset_to_ids.get(function.begin, []),
                    "context": [
                        format_instruction(instruction, image_base)
                        for instruction in instructions[first:last]
                    ],
                }
            )
    for target in arguments.inspect_rva:
        function = containing_function(ranges, begins, target)
        if function is None:
            function = FunctionRange(target, target + 64)
        instructions = function_instructions(pe, disassembler, function, image_base)
        target_index = next(
            (
                index
                for index, instruction in enumerate(instructions)
                if int(instruction.address - image_base) >= target
            ),
            len(instructions) - 1,
        )
        context_first = max(0, target_index - 120)
        context_last = min(len(instructions), target_index + 121)
        inspected_functions.append(
            {
                "rva": target,
                "mapped": True,
                "target": target,
                "begin": function.begin,
                "end": function.end,
                "address_library_ids": offset_to_ids.get(function.begin, []),
                "disassembly": [
                    format_instruction(instruction, image_base)
                    for instruction in instructions[:240]
                ],
                "target_context": [
                    format_instruction(instruction, image_base)
                    for instruction in instructions[context_first:context_last]
                ],
            }
        )

    report = {
        "executable": str(arguments.exe),
        "address_library": str(arguments.address_library),
        "image_base": image_base,
        "game_menu_base_vtables": sorted(base_vtables),
        "derived_primary_vtables": derived_vtables,
        "vtable_entries": vtable_entries,
        "derived_vtable_entries": derived_vtable_entries,
        "inspected_vtables": inspected_vtables,
        "triple_base_vtable_functions": triple_base_functions,
        "ranked_common_calls": candidate_details,
        "derived_vtable_functions": derived_function_records,
        "derived_factory_functions": derived_factory_functions,
        "inspected_functions": inspected_functions,
        "inspected_call_xrefs": inspected_call_xrefs,
    }
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("\nGameMenuBase vtables:")
    for identifier in GAME_MENU_BASE_VTABLE_IDS:
        print(f"  id={identifier} rva=0x{mappings[identifier]:08X}")
    print(f"\nFunctions referencing all three base vtables: {len(triple_base_functions)}")
    for item in triple_base_functions:
        print(
            f"  0x{item['begin']:08X}-0x{item['end']:08X} "
            f"ids={item['address_library_ids']} refs={len(item['references'])}"
        )
    print("\nCalls shared before derived primary-vtable writes:")
    for item in candidate_details:
        print(
            f"  0x{item['begin']:08X} ids={item['address_library_ids']} "
            f"menus={','.join(item['distinct_menus'])} sites={len(item['call_sites'])}"
        )
    print(f"\nWrote {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
