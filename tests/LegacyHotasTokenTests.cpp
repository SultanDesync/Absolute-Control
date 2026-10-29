#include "runtime/DeviceServiceDraft.h"
#include "runtime/LegacyHotasToken.h"
#include "fixtures/LegacyHotasTokenCases.h"

#include <array>
#include <iostream>
#include <set>
#include <string>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << __LINE__ << ": " << #expression << '\n'; return false; } } while (false)

namespace
{
    using namespace AbsoluteControl::Runtime::LegacyHotas;
    using LegacyHotasFixtures::kGuid;

    bool TestCorpus()
    {
        for (const auto& fixture : LegacyHotasFixtures::kCases) {
            const auto text = std::string(kGuid) + std::string(fixture.suffix);
            const auto decoded = Decode(text, fixture.dialect);
            if (decoded.status != fixture.status) std::cerr << "fixture: " << text << '\n';
            CHECK(decoded.status == fixture.status);
            CHECK(decoded.original == text);
            CHECK(decoded.binding.has_value() == (fixture.status == Status::Ok));
            if (!decoded.binding) continue;
            CHECK(decoded.binding->physicalId == kGuid);
            CHECK(decoded.binding->kind == fixture.kind);
            CHECK(decoded.binding->index == fixture.index);
            CHECK(decoded.binding->direction == fixture.direction);
            const auto producerText = EncodeCapture(*decoded.binding);
            CHECK(producerText);
            const auto roundTrip = Decode(*producerText, Dialect::HotasCapture);
            CHECK(roundTrip.binding == decoded.binding);
            // Encoding for a new capture must not mutate the original saved record.
            CHECK(decoded.original == text);
        }
        return true;
    }

    bool TestAllProducerChannels()
    {
        std::set<std::string> channels;
        std::uint32_t ambiguous{};
        const auto checkRoundTrip = [&channels](const Binding& binding, const std::string& expected) {
            const auto encoded = EncodeCapture(binding);
            if (!encoded || *encoded != std::string(kGuid) + expected) return false;
            const auto decoded = Decode(*encoded, Dialect::HotasCapture);
            const auto channel = ChannelId(binding);
            return decoded.binding == binding && channel && channels.insert(*channel).second;
        };
        for (std::uint32_t index = 0; index < 128; ++index) {
            const Binding binding{ std::string(kGuid), Kind::Button, index };
            CHECK(checkRoundTrip(binding, "@button:" + std::to_string(index + 1)));
            CHECK(Decode(*EncodeCapture(binding), Dialect::ZeroPower).binding == binding);
        }
        constexpr std::array<std::string_view, 4> directions{ "up", "right", "down", "left" };
        for (std::uint32_t index = 0; index < 4; ++index) {
            for (std::uint32_t direction = 0; direction < 4; ++direction) {
                const Binding binding{ std::string(kGuid), Kind::PovDirection, index, direction };
                CHECK(checkRoundTrip(binding, "@pov:" + std::to_string(index) + ":" +
                    std::string(directions[direction])));
                const auto encoded = *EncodeCapture(binding);
                const auto unspecified = Decode(encoded, Dialect::Unspecified);
                if (index > 0) {
                    CHECK(unspecified.status == Status::AmbiguousDialect);
                    CHECK(!unspecified.binding);
                    ++ambiguous;
                } else CHECK(unspecified.binding == binding);
                const auto subscriber = Decode(encoded, Dialect::ZeroPower);
                CHECK(subscriber.binding && subscriber.binding->index == (index ? index - 1 : 0));
                CHECK(subscriber.binding->direction == direction);
                // Flat aliases are unambiguous and preserve the subscriber's channel.
                const auto flat = std::string(kGuid) + "@pov:" + std::to_string(129 + index * 4 + direction);
                CHECK(Decode(flat, Dialect::ZeroPower).binding == binding);
            }
        }
        for (std::uint32_t index = 0; index < 8; ++index) {
            const Binding binding{ std::string(kGuid), Kind::Axis, index };
            CHECK(checkRoundTrip(binding, "@axis:0x3" + std::to_string(index)));
        }
        CHECK(channels.size() == 152 && ambiguous == 12);
        CHECK(channels.contains("button/0"));
        CHECK(channels.contains("axis/7"));
        CHECK(channels.contains("pov/3/direction/left"));
        return true;
    }

    bool TestIdentityAndPreservation()
    {
        const std::string lower = "{01234567-89ab-cdef-0123-456789abcdef}@button:001";
        const auto decoded = Decode(lower, Dialect::Unspecified);
        CHECK(decoded.status == Status::Ok && decoded.binding->physicalId == kGuid);
        CHECK(decoded.original == lower);
        CHECK(EncodeCapture(*decoded.binding) == std::string(kGuid) + "@button:1");
        auto other = *decoded.binding;
        other.physicalId = "{11234567-89AB-CDEF-0123-456789ABCDEF}";
        CHECK(other != *decoded.binding);
        CHECK(ChannelId(other) == ChannelId(*decoded.binding)); // Physical identity stays separate.
        for (const auto text : { "device@button:1", "#0@0x30", "guid:{bad}@1",
                 "{01234567-89AB-CDEF-0123-456789ABCDEZ}@button:1" }) {
            const auto opaque = Decode(text, Dialect::Unspecified);
            CHECK(opaque.status == Status::UnsupportedIdentity);
            CHECK(opaque.original == text && !opaque.binding);
        }
        for (const auto& text : { std::string{}, std::string("-1"), std::string("42"),
                 std::string(kGuid) + "@@button:1", std::string(256, 'x'),
                 std::string(kGuid) + std::string("@button:1\0junk", 14) }) {
            const auto invalid = Decode(text, Dialect::Unspecified);
            CHECK(invalid.status == Status::Invalid && !invalid.binding);
            CHECK(invalid.original == text);
        }
        auto invalid = *decoded.binding;
        invalid.index = 128;
        CHECK(!EncodeCapture(invalid) && !ChannelId(invalid));
        invalid = *decoded.binding;
        invalid.direction = 1;
        CHECK(!EncodeCapture(invalid));
        return true;
    }
}

int main()
{
    if (!TestCorpus() || !TestAllProducerChannels() || !TestIdentityAndPreservation()) return 1;
    std::cout << "Legacy HOTAS codec: " << LegacyHotasFixtures::kCases.size()
        << " fixtures, 152 producer channels and 12 ambiguous POV tokens passed.\n";
}
