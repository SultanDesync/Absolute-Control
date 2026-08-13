set_project("acp-generated-menu-compile-test")
set_version("0.1.0")
set_languages("c++23")

target("generated_menu_compile_test")
    set_kind("binary")
    add_includedirs("../../../include", "../../examples/generated")
    add_files("GeneratedMenuCompile.cpp")
