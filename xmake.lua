set_xmakever("3.0.0")

local product_version = "0.3.0-beta.1"

set_project("AbsoluteControlPanel")
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
    "src/runtime/ProbeRuntimeState.cpp",
    "src/runtime/RuntimeCompatibility.cpp",
    "src/scaleform/ScaleformMenuBridge.cpp",
    "src/ui/ControlPanelMenu.cpp",
    "src/ui/MenuAudioIntegration.cpp",
    "src/ui/MenuMessaging.cpp",
    "src/ui/PauseMenuIntegration.cpp"
}

local research_only_sources = {
    "src/ResearchInputCapture.cpp",
    "src/research/ResearchSupport.cpp"
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
        "Native shared configuration menu for Starfield SFSE plugins")

    add_installfiles(
        "docs/CURRENT-STATE.md",
        "docs/DECISIONS.md",
        "docs/MODULE-API.md",
        "docs/NATIVE-MENU-CONTRACT.md",
        "docs/AI-INTEGRATION-HARNESS.md",
        "docs/SDK-STATUS.md",
        "docs/SDK-RELEASE-PLAN.md",
        "docs/SUBSCRIBER-UI-STANDARD.md",
        "docs/SEMANTIC-UI-COMPOSITION-ARCHITECTURE.md",
        "docs/RUNTIME-UPDATE-RUNBOOK.md",
        "docs/TEST-MATRIX.md", {
        prefixdir = "Documentation"
    })
    add_installfiles("include/AbsoluteControlPanelAPI.h", "include/SlopAPI.h",
        "include/LiveComponentsExperimentalAPI.h",
        "include/AbsoluteControlCompositionExperimentalAPI.h", {
        prefixdir = "SDK"
    })
    add_installfiles("sdk/README.md", "sdk/CHANGELOG.md",
        "sdk/menu-definition.schema.json", {
        prefixdir = "SDK"
    })
    add_installfiles("sdk/tools/menu_codegen.py", {
        prefixdir = "SDK/tools"
    })
    add_installfiles("sdk/examples/absolute-head-tracking.menu.json", {
        prefixdir = "SDK/examples"
    })
    add_installfiles(
        "sdk/examples/generated/AbsoluteHeadTrackingMenu.generated.h", {
        prefixdir = "SDK/examples/generated"
    })
    add_installfiles("interface/dist/AbsoluteControlPanelMenu.swf", {
        prefixdir = "Interface"
    })
    add_installfiles("config/AbsoluteControlPanel.ini", {
        prefixdir = "SFSE/Plugins"
    })

    after_build(function()
        os.execv("powershell.exe", {
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", path.join(os.projectdir(),
                "tools", "build-artifacts", "New-BuildArtifactManifest.ps1"),
            "-Configuration", get_config("mode")
        })
    end)
end)

-- Explicit opt-in development artifact.  It composes the release host with the
-- mailbox/SendInput automation, DirectInput experiments, and
-- experimental LiveComponents ABI.  It is non-default, deliberately named, and
-- has none of the release target's SDK/interface/documentation install entries.
target("AbsoluteControlPanelResearchDev", function()
    set_default(false)
    set_basename("AbsoluteControlPanelResearchDev")
    configure_control_panel_plugin(
        "AbsoluteControlPanelResearchDev",
        "Opt-in Absolute Control Panel research and automation host")
    add_defines("ACP_ENABLE_RESEARCH_TOOLS=1", "ACP_ENABLE_LIVE_COMPONENTS_EXPERIMENTAL=1")
    for _, source in ipairs(research_only_sources) do
        add_files(source)
    end
    add_links("dinput8", "dxguid")

    after_build(function()
        os.execv("powershell.exe", {
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", path.join(os.projectdir(),
                "tools", "build-artifacts", "New-BuildArtifactManifest.ps1"),
            "-Configuration", get_config("mode"),
            "-ArtifactRole", "research-dev"
        })
    end)
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

target("native_decomposition_source_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("source_boundaries")
    add_files("tests/NativeDecompositionSourceTests.cpp")
end)

target("release_research_boundary_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("source_boundary")
    add_files("tests/ReleaseResearchBoundaryTests.cpp")
end)

target("release_research_artifact_test", function()
    set_kind("binary")
    set_default(false)
    add_deps("AbsoluteControlPanel", "AbsoluteControlPanelResearchDev")
    add_tests("binary_boundary")
    add_files("tests/ReleaseResearchArtifactTests.cpp")
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
