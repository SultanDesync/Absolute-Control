set_xmakever("3.0.0")

set_project("AbsoluteControl")
set_version("1.0.0-dev")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

add_rules("mode.debug", "mode.releasedbg", "mode.release")
add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")

-- libxse/commonlibsf, pinned by submodule. Which CommonLibSF lineage the
-- rewrite standardizes on is still open; see docs/history/COMMONLIBSF-COMPATIBILITY.md.
includes("external/commonlibsf")

-- Never deploy from a build. CommonLibSF's shared rule installs into the game
-- whenever XSE_SF_MODS_PATH or XSE_SF_GAME_PATH is set; copy output by hand.
rule("commonlib.plugin", function()
    after_build(function() end)
end)

-- The input runtime is plain C++ with no SFSE or game dependency, so the
-- plugin and every test link the same sources.
local runtime_sources = {
    "src/runtime/BackendRuntime.cpp",
    "src/runtime/DeviceStore.cpp",
    "src/runtime/DirectInputProvider.cpp"
}

target("AbsoluteControl", function()
    set_default(true)
    add_rules("commonlibsf.plugin", {
        name = "Absolute Control",
        author = "SultanDesync",
        description = "Flight controls for Starfield: HOTAS, mouse steering and head tracking on one input bus",
        options = {
            -- No game hooks yet. Revisit when the flight output path lands.
            address_library = false,
            layout_dependent = false,
            no_struct_use = true,
            sig_scanning = false
        }
    })
    set_pcxxheader("src/PCH.h")
    add_includedirs("include", "src")
    add_files("src/plugin.cpp")
    for _, source in ipairs(runtime_sources) do
        add_files(source)
    end
    add_syslinks("dinput8", "dxguid")
end)

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

target("headless_core_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("startup_authority_and_neutralization")
    add_includedirs("include")
    add_files("src/runtime/HeadlessCore.cpp", "tests/HeadlessCoreTests.cpp")
end)
