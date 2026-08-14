# CommonLibSF native-menu compatibility

> **Status:** Current runtime-specific compatibility record for Starfield 1.16.244. These mappings
> are internal implementation details, not provider ABI, and must be revalidated through
> [the runtime update runbook](RUNTIME-UPDATE-RUNBOOK.md) for every supported patch.

## Scope

The pinned CommonLibSF snapshot supports Starfield 1.16.244 broadly, but several native-menu
bindings in `RE/IDs.h` are intentionally unresolved. Its types remain useful structural hints;
they are not treated as ABI authority. Every mapping used by this probe was recovered from the
current executable and Address Library database, correlated against multiple shipped menu
constructors/vtables, and checked again inside the plugin before registration.

The build writes a generated `RE/IDs.h` overlay under ignored `build/.compat/`. Exact source-text
anchors make a changed dependency fail configuration instead of applying the mapping silently.
The submodule remains pristine.

## Verified Starfield 1.16.244 mappings

| Symbol | ID | RVA |
| --- | ---: | ---: |
| `GameMenuBase::ctor` | 130615 | `0x025516B0` |
| `GameMenuBase::Unk10` | 93620 | `0x01667080` |
| `GameMenuBase::Unk11` | 93621 | `0x016670C0` |
| `IMenu::dtor` | 130617 | `0x025518A0` |
| `IMenu::ShouldHandleEvent` | 91901 | `0x02553390` |
| `IMenu::OnThumbstickEvent` | 130633 | `0x02553670` |
| `IMenu::OnButtonEvent` | 130632 | `0x025533D0` |
| `IMenu::LoadMovie` | 130618 | `0x02551AB0` |
| `IMenu::ProcessMessage` | 130624 | `0x02552070` |
| `IMenu::Unk09` | 42815 | `0x00481670` |
| `IMenu::Unk0E` | 130622 | `0x02551D70` |
| `IMenu::Unk12` | 42816 | `0x00481680` |
| `IMenu::Unk13` | 39540 | `0x003AE910` |
| `IMenu::Unk19` | 130634 | `0x02553940` |
| `UI::IsMenuOpen` | 130475 | `0x02544EC0` |

The historical `GameMenuBase::ctor` ID 130577 is not the current constructor. The recovered
constructor at ID 130615 initializes the current menu fields, including render priority 6 at
byte `+0x110`. Shipped derived constructors also clear the tail byte at `+0x130`.

## Recovered lifecycle ABI

- Current menu factories receive raw return storage. A shipped factory constructs with reference
  count 1, adds one reference, and writes the raw pointer into the storage. Treating that storage
  as an initialized smart pointer corrupts ownership.
- The show processor calls virtual slot `IMenu::Unk0A`. The current base implementation is
  exactly `return uiMovie != nullptr`; false routes directly to cleanup.
- The successful-show path calls the active-menu insertion routine at RVA `0x0253EF10` from
  callsite `0x0254181C`.
- The current active render-order array is `UI + 0x430` (count) and `UI + 0x438` (data). It is
  sorted by the priority byte at `menu + 0x110`.
- In observed runs, PauseMenu used priority 11 and CursorMenu priority 20. The research overlay
  uses 19, placing it above PauseMenu while preserving the cursor layer.
- PauseMenu's observed runtime flags were `0x0800071B`; its constructor initially ORs
  `0x08000713`, with the cursor bit added by the lifecycle path. Absolute Control Panel now requests the full
  `0x0800071B` runtime mask explicitly because its custom registration path does not reliably
  receive PauseMenu's later cursor-bit mutation.

The plugin installs a fail-closed trace only after validating both the rel32 call opcode and its
expected target. It logs vanilla and research insertions before and after the engine routine; it
does not replace the routine's behavior.

## Scaleform findings

`IMenu::LoadMovie` successfully loads `Interface/AbsoluteControlPanelMenu.swf`. The usable root
is `_root`, a display object. A declared but uninitialized ActionScript `BGSCodeObj` property is
visible to `HasMember` while still null, so the native side creates and assigns a plain object
before mapping bridge functions. The verified sequence is:

1. SWF constructor and vector drawing complete;
2. native code creates/maps `BGSCodeObj` when necessary;
3. native code invokes `onCodeObjCreate`;
4. ActionScript calls native `ready(1)`; and
5. the engine inserts the menu into the active render array.

No ESM/ESP is needed for this DLL-to-factory-to-SWF path. A data plugin becomes relevant only if
later product work requires forms, quests, Papyrus, or a data-defined launch entry.

## Research-only limitations

The 312-byte custom object currently uses matching global C++ allocation/deallocation overrides.
The older declared Scaleform heap virtual interface was not safe in this runtime. Several
unresolved virtual slots remain conservative defaults. This compatibility layer is intentionally
single-runtime and should be removed when maintained definitions provide independently verified
bindings.
