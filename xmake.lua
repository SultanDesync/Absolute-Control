set_xmakever("3.0.0")

set_project("AbsoluteControlPanelResearch")
set_version("0.1.0")
set_license("GPL-3.0")
set_arch("x64")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")
set_defaultmode("releasedbg")

add_rules("mode.debug", "mode.releasedbg")

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

target("AbsoluteControlPanelResearch", function()
    add_defines("SLOP_EXPORTS")
    add_rules("commonlibsf.menu.compat")
    add_rules("commonlibsf.plugin", {
        name = "AbsoluteControlPanelResearch",
        author = "Absolute Control Panel Research contributors",
        description = "Standalone native Starfield menu research probe",
        options = {
            address_library = true,
            layout_dependent = true,
            no_struct_use = false,
            sig_scanning = false
        }
    })

    add_files("src/**.cpp")
    add_headerfiles("include/(**.h)")
    add_links("dinput8", "dxguid")
    -- This generated overlay shadows only RE/IDs.h. The upstream submodule remains pristine,
    -- and any upstream text change makes configuration fail instead of silently mispatching.
    add_includedirs(commonlibsf_menu_compat_include, "include")
    set_pcxxheader("include/PCH.h")

    add_installfiles("docs/RESEARCH-CHARTER.md", {
        prefixdir = "Documentation"
    })
    add_installfiles("docs/MODULE-API.md", "docs/AI-INTEGRATION-HARNESS.md", {
        prefixdir = "Documentation"
    })
    add_installfiles("include/SlopAPI.h", {
        prefixdir = "SDK"
    })
    add_installfiles("interface/dist/AbsoluteControlPanelMenu.swf", {
        prefixdir = "Interface"
    })
    add_installfiles("config/AbsoluteControlPanelResearch.ini", {
        prefixdir = "SFSE/Plugins"
    })
end)

target("probe_state_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("lifecycle")
    add_includedirs("include")
    add_files("tests/ProbeStateTests.cpp")
end)

target("slop_api_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("contract")
    add_defines("SLOP_EXPORTS")
    add_includedirs("include")
    add_files("src/MenuApiHost.cpp", "src/MenuSession.cpp", "tests/SlopApiTests.cpp")
end)
