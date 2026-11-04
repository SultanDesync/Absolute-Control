set_xmakever("3.0.0")

local product_version = "1.0.0-dev"

set_project("AbsoluteControl")
set_version(product_version)
add_defines('ACP_PRODUCT_VERSION="' .. product_version .. '"')
set_license("GPL-3.0")
set_arch("x64")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")
set_defaultmode("releasedbg")

add_rules("mode.debug", "mode.release", "mode.releasedbg")

-- Keep compiler-expanded source paths and PE debug records portable in any
-- distributable build. This maps the checkout root to a stable relative path
-- without hard-coding a developer name or workstation layout.
add_cxflags("/experimental:deterministic", "/pathmap:" .. os.projectdir() .. "=.", {
    tools = "cl",
    force = true
})

local commonlibsf_menu_compat_include = "build/.compat/commonlibsf-1.16.244"

rule("commonlibsf.menu.compat", function()
    on_load(function()
        local source_path = path.join(os.projectdir(), "external", "commonlibsf", "include", "RE", "IDs.h")
        local output_root = path.join(os.projectdir(), commonlibsf_menu_compat_include)
        local output_path = path.join(output_root, "RE", "IDs.h")
        local contents = assert(io.readfile(source_path), "Unable to read CommonLibSF RE/IDs.h")
        local replacements = {
            { "inline constexpr REL::ID ctor{ 0 };   // 130577", "inline constexpr REL::ID ctor{ 130615 };  // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID Unk10{ 0 };  // 141505", "inline constexpr REL::ID Unk10{ 93620 };  // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID Unk11{ 0 };  // 141506", "inline constexpr REL::ID Unk11{ 93621 };  // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID dtor{ 0 };               // 187216", "inline constexpr REL::ID dtor{ 130617 };               // destructor-chain correlated for 1.16.244" },
            { "inline constexpr REL::ID ShouldHandleEvent{ 0 };  // 187262", "inline constexpr REL::ID ShouldHandleEvent{ 91901 };  // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID OnThumbstickEvent{ 0 };  // 187235", "inline constexpr REL::ID OnThumbstickEvent{ 130633 };  // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID OnButtonEvent{ 0 };      // 187234", "inline constexpr REL::ID OnButtonEvent{ 130632 };      // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID LoadMovie{ 0 };          // 187240", "inline constexpr REL::ID LoadMovie{ 130618 };          // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID ProcessMessage{ 0 };     // 187247", "inline constexpr REL::ID ProcessMessage{ 130624 };     // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID Unk09{ 0 };              // 80440", "inline constexpr REL::ID Unk09{ 42815 };              // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID Unk0E{ 0 };              // 187242", "inline constexpr REL::ID Unk0E{ 130622 };              // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID Unk12{ 0 };              // 80451", "inline constexpr REL::ID Unk12{ 42816 };              // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID Unk13{ 0 };              // 76183", "inline constexpr REL::ID Unk13{ 39540 };              // vtable-correlated for 1.16.244" },
            { "inline constexpr REL::ID Unk19{ 0 };              // 187245", "inline constexpr REL::ID Unk19{ 130634 };              // vtable-correlated for 1.16.244" }
        }

        for _, replacement in ipairs(replacements) do
            local first = contents:find(replacement[1], 1, true)
            assert(first, "CommonLibSF compatibility anchor changed: " .. replacement[1])
            assert(not contents:find(replacement[1], first + #replacement[1], true),
                "CommonLibSF compatibility anchor is ambiguous: " .. replacement[1])
            contents = contents:sub(1, first - 1) .. replacement[2] ..
                contents:sub(first + #replacement[1])
        end

        os.mkdir(path.directory(output_path))
        io.writefile(output_path, contents)
    end)
end)

includes("external/commonlibsf")

-- Never deploy from a build. CommonLibSF's shared rule installs into the game
-- whenever XSE_SF_MODS_PATH or XSE_SF_GAME_PATH is set; copy output by hand.
rule("commonlib.plugin", function()
    after_build(function() end)
end)

-- Source ownership is deliberately explicit.  The release plugin must never gain a
-- research subsystem merely because a new .cpp file was added under src/.
local release_sources = {
    "src/CompositionApiHost.cpp",
    "src/CompositionRegistry.cpp",
    "src/ControlPanelModule.cpp",
    "src/EvidenceLog.cpp",
    "src/diagnostics/AsyncLineSink.cpp",
    "src/Main.cpp",
    "src/LiveComponentsRegistry.cpp",
    "src/MenuApiHost.cpp",
    "src/MenuInputRouter.cpp",
    "src/MenuSession.cpp",
    "src/NativeMenuProbe.cpp",
    "src/ProbeConfig.cpp",
    "src/input/NativeMenuInputAdapter.cpp",
    "src/input/PlatformInputServices.cpp",
    "src/interop/OSFUICompatibility.cpp",
    "src/runtime/ProbeRuntimeState.cpp",
    "src/runtime/BackendRuntime.cpp",
    "src/runtime/DeviceStore.cpp",
    "src/runtime/DirectInputProvider.cpp",
    "src/runtime/FlightSignals.cpp",
    "src/runtime/LegacyHotasToken.cpp",
    "src/runtime/RuntimeCompatibility.cpp",
    "src/runtime/ShipBindings.cpp",
    "src/runtime/ShipControlCore.cpp",
    "src/runtime/ShipControlLoop.cpp",
    "src/runtime/AxisCapture.cpp",
    "src/runtime/WalkSpeed.cpp",
    "src/runtime/WalkSpeedRuntime.cpp",
    "src/scaleform/ScaleformMenuBridge.cpp",
    "src/ui/ControlPanelMenu.cpp",
    "src/ui/MenuAudioIntegration.cpp",
    "src/ui/MenuMessaging.cpp",
    "src/ui/PauseMenuIntegration.cpp",
    "src/ui/PauseMenuPresentation.cpp",
    "src/ui/WalkGaugeMenu.cpp",
    "src/ui/WalkSpeedPage.cpp"
}

local function add_control_panel_core_sources()
    for _, source in ipairs(release_sources) do
        add_files(source)
    end
end

local function configure_control_panel_plugin(plugin_name, description)
    add_defines("SLOP_EXPORTS", "ABSOLUTE_CONTROL_PANEL_EXPORTS")
    add_rules("commonlibsf.menu.compat")
    add_rules("commonlibsf.plugin", {
        name = plugin_name,
        author = "Absolute Control suite contributors",
        description = description,
        options = {
            address_library = true,
            layout_dependent = true,
            no_struct_use = false,
            sig_scanning = false
        }
    })

    add_control_panel_core_sources()
    add_links("dinput8", "dxguid")
    add_headerfiles("include/(**.h)")
    -- This generated overlay shadows only RE/IDs.h. The upstream submodule remains pristine,
    -- and any upstream text change makes configuration fail instead of silently mispatching.
    add_includedirs(commonlibsf_menu_compat_include, "include")
    set_pcxxheader("include/PCH.h")
end

-- Canonical, installable product artifact.  This is the only default plugin target.
target("AbsoluteControlPanel", function()
    set_default(true)
    set_basename("AbsoluteControlPanel")
    configure_control_panel_plugin(
        "AbsoluteControlPanel",
        "Native in-game flight-control menu and input runtime for Starfield")

    add_installfiles("interface/dist/AbsoluteControlPanelMenu.swf", {
        prefixdir = "Interface"
    })
    add_installfiles("interface/dist/AbsoluteWalkGauge.swf", {
        prefixdir = "Interface"
    })
    add_installfiles("config/AbsoluteControlPanel.ini", {
        prefixdir = "SFSE/Plugins"
    })
end)

-- Test-hooks build of the same plugin, driven by tools/test. It keeps the product
-- file name so it replaces the installed host in a test profile instead of loading
-- beside it, and adds the research mailbox, title advance and guarded Continue.
-- It builds into its own directory and must never be packaged or released.
local test_hook_sources = {
    "src/research/PauseMenuDiagnostics.cpp",
    "src/research/ResearchSupport.cpp",
    "src/research/TestInputProvider.cpp",
    "src/research/TestMenuPage.cpp"
}

target("AbsoluteControlPanelTestHooks", function()
    set_default(false)
    set_basename("AbsoluteControlPanel")
    set_targetdir("build/test-hooks/$(mode)")
    configure_control_panel_plugin(
        "AbsoluteControlPanel",
        "Absolute Control TEST HOOKS build - not for release")
    add_defines("ACP_ENABLE_RESEARCH_TOOLS=1")
    for _, source in ipairs(test_hook_sources) do
        add_files(source)
    end
end)

-- Internal R2 codec/contract fixtures; no production provider is acquired.
target("device_lifecycle_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("snapshot_reconciliation_notifications_and_overflow")
    add_includedirs("include")
    add_files("src/runtime/DeviceStore.cpp", "tests/DeviceLifecycleTests.cpp")
end)

target("backend_runtime_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("native_data_plane_and_worker_lifetime")
    add_includedirs("include")
    add_files("src/runtime/BackendRuntime.cpp", "src/runtime/DeviceStore.cpp",
        "tests/BackendRuntimeTests.cpp")
end)

target("legacy_hotas_token_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("round_trip_and_dialect_ambiguity")
    add_includedirs("include")
    add_files("src/runtime/LegacyHotasToken.cpp", "tests/LegacyHotasTokenTests.cpp")
end)

target("ship_control_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("bindings_pilot_signal_and_routing")
    add_includedirs("include")
    add_files("src/runtime/DeviceStore.cpp", "src/runtime/LegacyHotasToken.cpp",
        "src/runtime/ShipBindings.cpp", "src/runtime/ShipControlCore.cpp",
        "tests/ShipControlTests.cpp")
end)

target("walk_speed_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("axis_tokens_selection_and_settings")
    add_includedirs("include")
    add_files("src/runtime/AxisCapture.cpp", "src/runtime/DeviceStore.cpp",
        "src/runtime/LegacyHotasToken.cpp", "src/runtime/ShipBindings.cpp",
        "src/runtime/ShipControlCore.cpp", "src/runtime/WalkSpeed.cpp", "tests/WalkSpeedTests.cpp")
end)

-- Internal R0/R1 harness only; this source is not in either plugin target.
target("headless_core_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("startup_authority_and_neutralization")
    add_includedirs("include")
    add_files("src/runtime/HeadlessCore.cpp", "tests/HeadlessCoreTests.cpp")
end)

target("probe_state_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("lifecycle")
    add_includedirs("include")
    add_files("tests/ProbeStateTests.cpp")
end)

target("lifecycle_diagnostics_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("service_and_sink_lifecycle")
    add_includedirs("include")
    add_files(
        "src/diagnostics/AsyncLineSink.cpp",
        "tests/LifecycleDiagnosticsTests.cpp")
end)

target("slop_api_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("contract")
    add_defines("SLOP_EXPORTS", "ABSOLUTE_CONTROL_PANEL_EXPORTS")
    add_includedirs("include")
    add_files("src/CompositionRegistry.cpp", "src/LiveComponentsRegistry.cpp",
        "src/MenuApiHost.cpp",
        "src/MenuInputRouter.cpp", "src/MenuSession.cpp", "tests/SlopApiTests.cpp")
end)

target("control_panel_module_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("settings_sorting_activation_and_registry")
    add_defines("SLOP_EXPORTS", "ABSOLUTE_CONTROL_PANEL_EXPORTS")
    add_includedirs("include")
    add_files(
        "src/CompositionRegistry.cpp",
        "src/ControlPanelModule.cpp",
        "src/LiveComponentsRegistry.cpp",
        "src/MenuApiHost.cpp",
        "src/ProbeConfig.cpp",
        "tests/ControlPanelModuleTests.cpp")
end)

target("host_hardening_stress_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("registry_refresh_snapshot_stress")
    add_defines("SLOP_EXPORTS", "ABSOLUTE_CONTROL_PANEL_EXPORTS")
    add_includedirs("include")
    add_files(
        "src/MenuApiHost.cpp",
        "src/CompositionRegistry.cpp",
        "src/LiveComponentsRegistry.cpp",
        "src/MenuSession.cpp",
        "tests/HostHardeningStressTests.cpp")
end)

target("scaleform_bridge_source_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("source_contract")
    add_files("tests/ScaleformBridgeSourceTests.cpp")
end)

target("continue_confirmation_policy_test", function()
    set_kind("binary")
    set_default(false)
    add_files("tests/ContinueConfirmationPolicyTests.cpp")
    add_includedirs("include")
    add_tests("guarded_safe_save_confirmation")
end)

target("pause_entry_policy_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("coexistence_ownership_and_action_dispatch")
    add_includedirs("include")
    add_files("tests/PauseEntryPolicyTests.cpp")
end)

target("bridge_protocol_state_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("version_and_publication_acceptance")
    add_includedirs("include")
    add_files("tests/BridgeProtocolStateTests.cpp")
end)

target("native_decomposition_source_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("source_boundaries")
    add_files("tests/NativeDecompositionSourceTests.cpp")
end)

target("live_components_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("experimental_contract")
    add_defines("ABSOLUTE_CONTROL_PANEL_EXPORTS")
    add_includedirs("include")
    add_files("src/LiveComponentsRegistry.cpp", "tests/LiveComponentsTests.cpp")
end)

target("composition_registry_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("c0_model_validation_fallback_and_leases")
    add_defines("ABSOLUTE_CONTROL_PANEL_EXPORTS")
    add_includedirs("include")
    add_files(
        "src/CompositionRegistry.cpp",
        "src/LiveComponentsRegistry.cpp",
        "tests/CompositionRegistryTests.cpp")
end)

target("semantic_composition_integration_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("c2_six_card_live_session_input_and_performance")
    add_defines("SLOP_EXPORTS", "ABSOLUTE_CONTROL_PANEL_EXPORTS")
    add_includedirs("include")
    add_files(
        "src/CompositionApiHost.cpp",
        "src/CompositionRegistry.cpp",
        "src/LiveComponentsRegistry.cpp",
        "src/MenuApiHost.cpp",
        "src/MenuInputRouter.cpp",
        "src/MenuSession.cpp",
        "tests/SemanticCompositionIntegrationTests.cpp")
end)
