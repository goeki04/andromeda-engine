// Guid text form: it is what asset_db.json and the scene files store, so the format is part of the
// project data. A change here would make every saved Guid unreadable.

#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "a_Guid.hpp"

using Andromeda::Guid;

namespace {
    Guid guidFromBytes(std::array<Andromeda::u8, 16> bytes) {
        return Guid{ bytes };
    }
}

TEST(Guid, ToString_WritesBytesInStorageOrderAsLowercaseHex) {
    const Guid guid = guidFromBytes({ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                      0x08, 0x09, 0x0A, 0x0B, 0xAC, 0xDE, 0xEF, 0xFF });

    EXPECT_EQ(guid.toString(), "000102030405060708090a0bacdeefff");
}

TEST(Guid, FromString_ReadsBackWhatToStringWrote) {
    const std::vector<Guid> guids = {
        guidFromBytes({}),
        guidFromBytes({ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }),
        guidFromBytes({ 0x96, 0x9c, 0x6c, 0x10, 0xad, 0x37, 0x4b, 0x52, 0x8d, 0x49, 0xea, 0x4c, 0xab, 0x8b, 0x5f, 0xce }),
    };

    for (const Guid& guid : guids) {
        SCOPED_TRACE(guid.toString());
        const std::optional<Guid> parsed = Guid::fromString(guid.toString());
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, guid);
    }
}

TEST(Guid, FromString_AcceptsUppercase) {
    // A hand-edited file or another tool may write uppercase hex.
    EXPECT_EQ(Guid::fromString("969C6C10AD374B528D49EA4CAB8B5FCE"),
              Guid::fromString("969c6c10ad374b528d49ea4cab8b5fce"));
}

TEST(Guid, FromString_RejectsAnythingButExactly32HexDigits) {
    const std::vector<std::string> malformed = {
        "",
        "969c6c10ad374b528d49ea4cab8b5fc",                 // 31 characters
        "969c6c10ad374b528d49ea4cab8b5fcee",               // 33 characters
        "969c6c10-ad37-4b52-8d49-ea4cab8b5fce",            // the dashed form other tools use
        "g69c6c10ad374b528d49ea4cab8b5fce",                // not a hex digit
        "9g9c6c10ad374b528d49ea4cab8b5fce",                // only the second digit of a pair is bad
        "+69c6c10ad374b528d49ea4cab8b5fce",                // from_chars would otherwise read a sign
        " 69c6c10ad374b528d49ea4cab8b5fce",                // leading whitespace
    };

    for (const std::string& text : malformed) {
        SCOPED_TRACE("'" + text + "'");
        EXPECT_FALSE(Guid::fromString(text).has_value());
    }
}

TEST(Guid, KeysThatDifferOnlyInTheLastByteStaySeparateInAMap) {
    // std::hash<Guid> only looks at the first eight bytes, so these two collide on purpose:
    // the map must still tell them apart through operator==.
    Guid a = guidFromBytes({});
    Guid b = a;
    b.bytes[15] = 1;

    std::unordered_map<Guid, int> map;
    map[a] = 1;
    map[b] = 2;

    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map.at(a), 1);
    EXPECT_EQ(map.at(b), 2);
}
