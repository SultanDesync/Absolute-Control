// Prints what the one-time import would make of AbsoluteHOTAS settings files, without
// touching anything: import_preview AbsoluteHOTAS.ini [AbsoluteHOTAS_Custom.ini ...]
#include "runtime/SettingsImport.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv)
{
    using namespace AbsoluteControl::Runtime;
    if (argc < 2) {
        std::cerr << "usage: import_preview <hotas ini> [later ini ...]\n";
        return 2;
    }
    IniValues hotas;
    for (int index = 1; index < argc; ++index) {
        std::ifstream stream{ argv[index], std::ios::binary };
        if (!stream) {
            std::cerr << "can't read " << argv[index] << '\n';
            return 1;
        }
        hotas.Merge(std::string{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() });
    }
    const auto imported = ImportHotasSettings(hotas);
    std::string section;
    for (const auto& entry : imported.entries) {
        if (entry.section != section) {
            section = entry.section;
            std::cout << "\n[" << section << "]\n";
        }
        std::cout << entry.key << " = " << entry.value << '\n';
    }
    std::cout << "\n; not carried over: " << imported.dropped.size() << '\n';
    for (const auto& line : imported.dropped) std::cout << ";   " << line << '\n';
    return 0;
}
