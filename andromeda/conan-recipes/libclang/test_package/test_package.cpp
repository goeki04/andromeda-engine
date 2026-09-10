// Smoke test for the repackaged libclang: parse a struct out of a header we
// write ourselves, so the test depends on nothing but the package.
#include <clang-c/Index.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace {
    int fieldCount = 0;

    CXChildVisitResult countFields(CXCursor cursor, CXCursor, CXClientData) {
        if (clang_getCursorKind(cursor) == CXCursor_FieldDecl) {
            ++fieldCount;
        }
        return CXChildVisit_Recurse;
    }
}

int main() {
    const char* path = "test_package_input.hpp";
    {
        std::ofstream out(path);
        out << "struct Probe { int a; float b; double c; };\n";
    }

    CXString version = clang_getClangVersion();
    std::printf("libclang reports: %s\n", clang_getCString(version));
    clang_disposeString(version);

    const char* args[] = { "-x", "c++", "-std=c++20" };

    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = nullptr;
    const CXErrorCode error = clang_parseTranslationUnit2(
        index, path, args, 3, nullptr, 0, CXTranslationUnit_SkipFunctionBodies, &unit);

    if (error != CXError_Success || unit == nullptr) {
        std::printf("FAILED: clang_parseTranslationUnit2 returned %d\n", (int)error);
        clang_disposeIndex(index);
        return 1;
    }

    clang_visitChildren(clang_getTranslationUnitCursor(unit), countFields, nullptr);
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);

    if (fieldCount != 3) {
        std::printf("FAILED: expected 3 fields in struct Probe, found %d\n", fieldCount);
        return 1;
    }

    std::printf("OK: parsed struct Probe, found %d fields\n", fieldCount);
    return 0;
}
