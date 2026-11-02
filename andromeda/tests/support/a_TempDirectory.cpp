#include "a_TempDirectory.hpp"
#include "a_filesystem.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>

namespace Andromeda::Test {
    namespace {
        std::string currentTestName() {
            const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
            if (info == nullptr) {
                return "outside_test";
            }
            // Parameterized tests are called "Suite/Name/0"; '/' must not end up in a folder name.
            std::string name = std::string(info->test_suite_name()) + "." + info->name();
            for (char& c : name) {
                if (c == '/' || c == '\\') {
                    c = '_';
                }
            }
            return name;
        }
    }

    TempDirectory::TempDirectory() {
        std::random_device random;
        const std::filesystem::path base = std::filesystem::temp_directory_path() / "andromeda_tests";
        m_Path = base / (currentTestName() + "_" + std::to_string(random()));
        std::filesystem::create_directories(m_Path);
    }

    TempDirectory::~TempDirectory() {
        // error_code overload: cleaning up must never turn a passing test into a crash.
        std::error_code ec;
        std::filesystem::remove_all(m_Path, ec);
    }

    std::filesystem::path TempDirectory::operator/(const std::filesystem::path& relative) const {
        return m_Path / relative;
    }

    std::filesystem::path TempDirectory::write(const std::filesystem::path& relative, const std::string_view content) const {
        const std::filesystem::path file = m_Path / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out) {
            throw std::runtime_error("TempDirectory could not write " + Filesystem::toUtf8(file));
        }
        return file;
    }

    std::string TempDirectory::read(const std::filesystem::path& relative) const {
        std::ifstream in(m_Path / relative, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
}
