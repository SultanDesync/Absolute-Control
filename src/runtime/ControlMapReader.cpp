#include "runtime/ControlMapReader.h"

#include <Windows.h>

#include <algorithm>
#include <fstream>
#include <iterator>

namespace AbsoluteControl::Runtime::ControlMap
{
    namespace
    {
        std::uint32_t ReadU32(const std::uint8_t* p)
        {
            return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
                (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
        }

        std::size_t FindZero(std::span<const std::uint8_t> bytes, std::size_t start, std::size_t end)
        {
            for (std::size_t index = start; index < end; ++index) {
                if (bytes[index] == 0) return index;
            }
            return end;
        }

        // Taken from the virtual key, because a scan code alone is ambiguous: the Down arrow
        // and Numpad 2 share 0x50.
        bool IsExtendedKey(UINT key)
        {
            switch (key) {
            case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
            case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
            case VK_INSERT: case VK_DELETE: case VK_NUMLOCK: case VK_DIVIDE:
            case VK_RCONTROL: case VK_RMENU:
                return true;
            default:
                return false;
            }
        }
    }

    File Parse(std::span<const std::uint8_t> bytes)
    {
        File file;
        std::size_t offset{};
        while (offset < bytes.size()) {
            if (offset + 3 > bytes.size() || bytes[offset] != 0x03) return file;
            const std::size_t length = (static_cast<std::size_t>(bytes[offset + 1]) << 8) | bytes[offset + 2];
            if (length < 3 || offset + length > bytes.size()) return file;

            const std::size_t end = offset + length;
            std::size_t cursor = offset + 3;
            while (cursor < end) {
                const auto contextEnd = FindZero(bytes, cursor, end);
                if (contextEnd == end) break; // Trailing padding.
                const auto actionStart = contextEnd + 1;
                const auto actionEnd = FindZero(bytes, actionStart, end);
                const auto payload = actionEnd + 1;
                if (actionEnd == end || payload + 8 > end) return file;

                Record record;
                record.context.assign(reinterpret_cast<const char*>(&bytes[cursor]), contextEnd - cursor);
                record.action.assign(reinterpret_cast<const char*>(&bytes[actionStart]), actionEnd - actionStart);
                record.device = ReadU32(&bytes[payload]);
                record.token = static_cast<std::uint16_t>(bytes[payload + 4] | (bytes[payload + 5] << 8));
                record.flag = bytes[payload + 6];
                if (record.device == 1) file.records.push_back(std::move(record));
                cursor = payload + 8;
            }
            offset = end;
        }
        file.valid = true;
        return file;
    }

    File ReadFile(const std::filesystem::path& path)
    {
        std::ifstream stream{ path, std::ios::binary };
        if (!stream) return {};
        const std::vector<char> text{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        return Parse({ reinterpret_cast<const std::uint8_t*>(text.data()), text.size() });
    }

    Output TokenToOutput(std::uint16_t token)
    {
        if (token == kUnboundToken) return {};
        if (token <= 3) return { ShipActionOutputKind::Mouse, static_cast<std::uint16_t>(token + 1), false };
        // The game writes the virtual key in the low byte: K is 0x004B, Left is 0x0025.
        const UINT key = token & 0xFFu;
        const UINT scan = ::MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
        if (scan == 0) return {};
        return { ShipActionOutputKind::Keyboard, static_cast<std::uint16_t>(scan & 0xFFu), IsExtendedKey(key) };
    }

    bool HasPrimary(std::span<const Record> records, std::string_view context, std::string_view action)
    {
        return std::ranges::any_of(records, [&](const Record& record) {
            return record.IsPrimary() && record.context == context && record.action == action;
        });
    }

    Output ResolveBinding(std::span<const Record> records, std::string_view context,
        std::string_view action, Output vanilla)
    {
        const Record* primary{};
        const Record* alternate{};
        for (const auto& record : records) {
            if (record.context != context || record.action != action) continue;
            if (record.IsPrimary()) {
                if (!primary) primary = &record;
            } else if (!record.IsUnbound() && !alternate) {
                alternate = &record;
            }
        }
        if (!primary) return vanilla;
        if (!primary->IsUnbound()) {
            // A key we can't reproduce: vanilla is the best guess.
            const auto output = TokenToOutput(primary->token);
            return output.kind != ShipActionOutputKind::None ? output : vanilla;
        }
        if (alternate) {
            const auto output = TokenToOutput(alternate->token);
            if (output.kind != ShipActionOutputKind::None) return output;
        }
        return {};
    }
}
