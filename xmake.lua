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

includes("external/commonlibsf")

target("AbsoluteControlPanelResearch", function()
    add_rules("commonlibsf.plugin", {
        name = "AbsoluteControlPanelResearch",
        author = "jordan hachey",
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
    add_includedirs("include")
    set_pcxxheader("include/PCH.h")

    add_installfiles("docs/RESEARCH-CHARTER.md", {
        prefixdir = "Documentation"
    })
end)

target("probe_state_test", function()
    set_kind("binary")
    set_default(false)
    add_tests("lifecycle")
    add_includedirs("include")
    add_files("tests/ProbeStateTests.cpp")
end)
