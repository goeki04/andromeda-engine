#include <gtest/gtest.h>

#include "a_Reflector.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

    // Writes a header into the test's temp dir and removes it again, so the
    // tests depend on nothing in the source tree.
    class ScopedHeader {
    public:
        ScopedHeader(const std::string& name, const std::string& contents) {
            path_ = std::filesystem::temp_directory_path() / name;
            std::ofstream out(path_);
            out << contents;
        }

        ~ScopedHeader() {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }

        std::string path() const { return path_.string(); }

    private:
        std::filesystem::path path_;
    };
}

#ifdef ANDROMEDA_HAS_LIBCLANG

TEST(Reflector, FindsFieldsOfAPlainStruct) {
    ScopedHeader header("andromeda_reflect_plain.hpp",
        "struct Probe {\n"
        "    int count;\n"
        "    float ratio;\n"
        "};\n");

    const auto info = Andromeda::Reflector::reflect_struct(header.path(), "Probe");

    ASSERT_TRUE(info.found);
    ASSERT_EQ(info.fields.size(), 2u);
    EXPECT_EQ(info.fields[0].name, "count");
    EXPECT_EQ(info.fields[0].type, "int");
    EXPECT_EQ(info.fields[1].name, "ratio");
    EXPECT_EQ(info.fields[1].type, "float");
}

// The ECS components live in namespace Andromeda::ECS::Component, so the
// visitor has to descend into namespaces rather than only look at top level.
TEST(Reflector, FindsStructInsideNestedNamespace) {
    ScopedHeader header("andromeda_reflect_nested.hpp",
        "namespace Andromeda::ECS::Component {\n"
        "    struct [[Andromeda::Undo]] Tag {\n"
        "        int id;\n"
        "    };\n"
        "}\n");

    const auto info = Andromeda::Reflector::reflect_struct(header.path(), "Tag");

    ASSERT_TRUE(info.found);
    ASSERT_EQ(info.fields.size(), 1u);
    EXPECT_EQ(info.fields[0].name, "id");
}

// A forward declaration carries the right name but no fields. Reflecting it
// must not report success with an empty field list.
TEST(Reflector, IgnoresForwardDeclarationAndUsesTheDefinition) {
    ScopedHeader header("andromeda_reflect_fwd.hpp",
        "struct Probe;\n"
        "struct Probe { int value; };\n");

    const auto info = Andromeda::Reflector::reflect_struct(header.path(), "Probe");

    ASSERT_TRUE(info.found);
    ASSERT_EQ(info.fields.size(), 1u);
    EXPECT_EQ(info.fields[0].name, "value");
}

TEST(Reflector, ReportsNotFoundForAnUnknownStruct) {
    ScopedHeader header("andromeda_reflect_missing.hpp", "struct Probe { int value; };\n");

    const auto info = Andromeda::Reflector::reflect_struct(header.path(), "NotInThisFile");

    EXPECT_FALSE(info.found);
    EXPECT_TRUE(info.fields.empty());
}

// This is the trap worth pinning down: with an unresolved include, clang
// recovers unknown types as `int` instead of failing. The parse still succeeds,
// the field names are right, but the types are silently wrong - which is why
// reflect_struct takes extraArgs for the include directories.
TEST(Reflector, ResolvesRealTypesOnlyWithIncludeDirectories) {
    ScopedHeader dependency("andromeda_reflect_types.hpp",
        "#pragma once\n"
        "struct Vec3 { float x, y, z; };\n");
    ScopedHeader header("andromeda_reflect_uses_types.hpp",
        "#include \"andromeda_reflect_types.hpp\"\n"
        "struct Probe { Vec3 position; };\n");

    const auto without = Andromeda::Reflector::reflect_struct(header.path(), "Probe");
    ASSERT_TRUE(without.found);
    ASSERT_EQ(without.fields.size(), 1u);
    EXPECT_EQ(without.fields[0].name, "position");

    const std::string includeFlag =
        "-I" + std::filesystem::temp_directory_path().string();
    const auto with = Andromeda::Reflector::reflect_struct(header.path(), "Probe", { includeFlag });
    ASSERT_TRUE(with.found);
    ASSERT_EQ(with.fields.size(), 1u);
    EXPECT_EQ(with.fields[0].type, "Vec3");
}

#else // ANDROMEDA_HAS_LIBCLANG

TEST(Reflector, ReturnsEmptyResultWithoutLibclang) {
    const auto info = Andromeda::Reflector::reflect_struct("anything.hpp", "Probe");
    EXPECT_FALSE(info.found);
    EXPECT_TRUE(info.fields.empty());
}

#endif // ANDROMEDA_HAS_LIBCLANG
