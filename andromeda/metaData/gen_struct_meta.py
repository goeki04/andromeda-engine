#!/usr/bin/env python3
"""
gen_struct_meta.py - generate compile-time metadata for plain C++ structs.

Scans one or more headers (default:
modules/definitions/renderer/a_particle_group.hpp), extracts every top-level
struct together with its public data members - name, type, default initializer
and the attached Doxygen comment - and writes a header that specializes
Andromeda::Meta::StructInfo<T> for each struct that was found.

The generated metadata lets the editor UI, the serializer and the undo system
walk a struct's fields generically instead of hand-listing them in three
different places:

    Andromeda::Meta::forEachField<Andromeda::ParticleGroup>([](auto const& f) {
        spdlog::info("{} : {} - {}", f.name, f.typeName, f.doc);
    });

Usage:
    python metaData/gen_struct_meta.py
    python metaData/gen_struct_meta.py --input a.hpp --input b.hpp --output out.hpp
    python metaData/gen_struct_meta.py --struct ParticleGroup

Limitations (deliberate): this is a line-based scanner, not a real C++ frontend.
It understands the struct convention used in this project - one member per line,
optional default initializer, optional ///< comment. Templates, macros inside
member declarations and bitfields are not supported; static, constexpr, typedef
and non-public members are skipped.

The output file is only rewritten when its content actually changes. That keeps
a comment-only edit in the source header from triggering a rebuild of every
translation unit that includes the generated header; the cost is that the build
system re-runs this script (a few milliseconds) on every build.
"""

import argparse
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

DEFAULT_INPUTS = ["modules/definitions/renderer/a_particle_group.hpp"]
DEFAULT_OUTPUT = "modules/definitions/renderer/generated_particle_group_meta.hpp"

TAG = "-- StructMeta:"

NAMESPACE_RE = re.compile(r"^\s*namespace\s+([A-Za-z_][\w:]*)\s*\{")
STRUCT_RE = re.compile(
    r"\b(?:struct|class)\s+"
    r"(?P<attrs>(?:\[\[[^\]]*\]\]\s*)*)"
    r"(?P<name>[A-Za-z_]\w*)\s*"
    r"(?P<bases>:[^{;]*)?\{"
)
DECL_RE = re.compile(
    r"^(?P<type>.*?[\w>\]&*])\s+"
    r"(?P<names>[A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)$"
)
ACCESS_RE = re.compile(r"^\s*(public|private|protected)\s*:\s*$")

# Statements that can never be a data member.
SKIP_LEADING = (
    "static", "inline", "constexpr", "consteval", "constinit", "using",
    "typedef", "friend", "template", "enum", "union",
    "return", "explicit", "virtual", "operator",
)
CPP_KEYWORDS = {"return", "if", "else", "for", "while", "switch", "case", "default"}


# ---------------------------------------------------------------------------
# Text helpers
# ---------------------------------------------------------------------------

def split_line_comment(line):
    """Split a line into (code, comment), ignoring '//' inside string literals."""
    in_str = None
    i = 0
    while i < len(line):
        c = line[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == in_str:
                in_str = None
        elif c in ('"', "'"):
            in_str = c
        elif c == "/" and line[i + 1:i + 2] == "/":
            return line[:i], line[i + 2:]
        i += 1
    return line, None


def blank_strings(code):
    """Blank out string/char contents so braces can be counted safely."""
    out = []
    in_str = None
    i = 0
    while i < len(code):
        c = code[i]
        if in_str:
            if c == "\\":
                out.append("__")
                i += 2
                continue
            if c == in_str:
                in_str = None
                out.append(c)
            else:
                out.append("_")
        elif c in ('"', "'"):
            in_str = c
            out.append(c)
        else:
            out.append(c)
        i += 1
    return "".join(out)


def clean_doc(text):
    """Turn '///< Foo' or '/** @brief Foo */' into a single-line description."""
    if not text:
        return ""
    text = text.replace("*/", " ")
    lines = []
    for raw in text.splitlines():
        s = raw.strip()
        s = re.sub(r"^/\*+<?", "", s)
        s = re.sub(r"^[/*!<]+", "", s)
        s = re.sub(r"^\s*@brief\s+", "", s)
        s = s.strip()
        if s:
            lines.append(s)
    return " ".join(lines).strip()


def split_initializer(stmt):
    """Split 'vec3 v = {1,2,3}' into ('vec3 v', '{1,2,3}'), bracket-depth aware."""
    angle = paren = brace = 0
    i = 0
    while i < len(stmt):
        c = stmt[i]
        if c == "<":
            angle += 1
        elif c == ">":
            angle = max(0, angle - 1)
        elif c == "(":
            paren += 1
        elif c == ")":
            paren = max(0, paren - 1)
        elif c == "{":
            if angle == 0 and paren == 0 and brace == 0:
                return stmt[:i].strip(), stmt[i:].strip()
            brace += 1
        elif c == "}":
            brace = max(0, brace - 1)
        elif c == "=" and angle == 0 and paren == 0 and brace == 0:
            prev = stmt[i - 1:i]
            nxt = stmt[i + 1:i + 2]
            if nxt != "=" and prev not in ("=", "!", "<", ">", "+", "-", "*", "/"):
                return stmt[:i].strip(), stmt[i + 1:].strip()
        i += 1
    return stmt.strip(), None


def cpp_string(value):
    """Escape a Python string so it can be emitted as a C++ string literal."""
    return (value.replace("\\", "\\\\")
                 .replace('"', '\\"')
                 .replace("\n", " ")
                 .replace("\r", " "))


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

def parse_member(stmt, doc):
    """Turn a ';'-terminated statement into member entries, or None."""
    stmt = stmt.strip().rstrip(";").strip()
    if not stmt:
        return None

    lowered = stmt.lower()
    for word in SKIP_LEADING:
        if lowered.startswith(word):
            return None
    if "(" in stmt.split("=")[0]:  # function declaration
        return None

    decl, default = split_initializer(stmt)
    if default is not None:
        default = default.strip().rstrip(";").strip()
        if default.startswith("{") and not default.endswith("}"):
            default = default + "}"

    match = DECL_RE.match(decl)
    if not match:
        return None

    ctype = " ".join(match.group("type").split())
    names = [n.strip() for n in match.group("names").split(",")]
    if not ctype or any(n in CPP_KEYWORDS for n in names):
        return None
    if ctype.endswith("::") or ctype in CPP_KEYWORDS:
        return None

    entries = []
    for index, name in enumerate(names):
        entries.append({
            "name": name,
            "type": ctype,
            "doc": doc or "",
            # For 'float x, y, z = 1.f' the default belongs to the last name only.
            "default": default if (default and index == len(names) - 1) else "",
        })
    return entries


def parse_header(path):
    """Return a list of structs: {name, namespace, attrs, doc, fields[]}."""
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = handle.read().splitlines()

    structs = []
    ns_stack = []          # [(depth, name)]
    depth = 0
    pending_doc = ""
    block_buf = None       # currently open /* */ comment
    current = None         # struct being parsed
    struct_depth = 0
    access = "public"
    buffer = ""

    for raw in lines:
        line = raw

        # --- block comments --------------------------------------------------
        if block_buf is not None:
            end = line.find("*/")
            if end == -1:
                block_buf.append(line)
                continue
            block_buf.append(line[:end])
            pending_doc = clean_doc("\n".join(block_buf))
            block_buf = None
            line = line[end + 2:]

        while "/*" in line:
            start = line.find("/*")
            end = line.find("*/", start + 2)
            if end == -1:
                block_buf = [line[start + 2:]]
                line = line[:start]
                break
            pending_doc = clean_doc(line[start + 2:end])
            line = line[:start] + " " + line[end + 2:]

        code, comment = split_line_comment(line)
        trailing_doc = clean_doc(comment) if comment else ""
        if not code.strip():
            # Comment-only line: remember it as documentation for the next member.
            if trailing_doc:
                pending_doc = (pending_doc + " " + trailing_doc).strip() if pending_doc else trailing_doc
            continue

        counted = blank_strings(code)
        opens = counted.count("{")
        closes = counted.count("}")

        # --- outside of a struct ---------------------------------------------
        if current is None:
            ns_match = NAMESPACE_RE.match(code)
            if ns_match:
                ns_stack.append((depth, ns_match.group(1)))
                depth += opens - closes
                pending_doc = ""
                continue

            st_match = STRUCT_RE.search(code)
            if st_match and ";" not in code.split("{")[0]:
                is_class = re.search(r"\bclass\s", code[:st_match.end()]) is not None
                access = "private" if is_class else "public"
                current = {
                    "name": st_match.group("name"),
                    "namespace": "::".join(n for _, n in ns_stack),
                    "attrs": (st_match.group("attrs") or "").strip(),
                    "doc": pending_doc,
                    "fields": [],
                }
                struct_depth = depth
                depth += opens - closes
                buffer = ""
                pending_doc = ""
                continue

            depth += opens - closes
            while ns_stack and depth <= ns_stack[-1][0]:
                ns_stack.pop()
            pending_doc = ""
            continue

        # --- inside a struct --------------------------------------------------
        new_depth = depth + opens - closes
        member_depth = struct_depth + 1

        if new_depth <= struct_depth:
            structs.append(current)
            current = None
            depth = new_depth
            buffer = ""
            pending_doc = ""
            while ns_stack and depth <= ns_stack[-1][0]:
                ns_stack.pop()
            continue

        if depth > member_depth or new_depth > member_depth:
            # Method body or nested block - not a member, skip it.
            depth = new_depth
            buffer = ""
            pending_doc = ""
            continue

        depth = new_depth

        access_match = ACCESS_RE.match(code)
        if access_match:
            access = access_match.group(1)
            buffer = ""
            pending_doc = ""
            continue

        buffer = (buffer + " " + code.strip()).strip()

        if buffer.rstrip().endswith(";"):
            doc = trailing_doc or pending_doc
            entries = parse_member(buffer, doc) if access == "public" else None
            if entries:
                current["fields"].extend(entries)
            buffer = ""
            pending_doc = ""
        elif "{" in buffer or "}" in buffer:
            buffer = ""
            pending_doc = ""

    if current is not None:          # unbalanced file - keep what we have
        structs.append(current)

    return structs


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------

PRELUDE = '''#pragma once

// ============================================================================
//  AUTO-GENERATED FILE - DO NOT EDIT BY HAND
//  Produced by metaData/gen_struct_meta.py
//  Sources:
{sources}
//  Changes to the source headers are picked up on the next build
//  (target: generate_ecs_metadata).
// ============================================================================

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

{includes}

namespace Andromeda::Meta {{

    /** @brief Description of a single data member of a reflected struct. */
    template <typename Owner, typename Member>
    struct FieldInfo {{
        using owner_type = Owner;
        using member_type = Member;

        std::string_view name;           ///< Member name, exactly as written in the header.
        std::string_view typeName;       ///< Type as source text, e.g. "vec3".
        std::string_view doc;            ///< Doxygen comment of the member ("" if none).
        std::string_view defaultLiteral; ///< Default initializer as text ("" if none).
        Member Owner::* pointer;         ///< Pointer-to-member for generic access.

        constexpr const Member& get(const Owner& owner) const noexcept {{ return owner.*pointer; }}
        constexpr Member& get(Owner& owner) const noexcept {{ return owner.*pointer; }}
    }};

    template <typename Owner, typename Member>
    constexpr FieldInfo<Owner, Member> makeField(std::string_view name,
                                                 std::string_view typeName,
                                                 std::string_view doc,
                                                 std::string_view defaultLiteral,
                                                 Member Owner::* pointer) noexcept {{
        return FieldInfo<Owner, Member>{{name, typeName, doc, defaultLiteral, pointer}};
    }}

    /** @brief Primary template - specialized below for every scanned struct. */
    template <typename T>
    struct StructInfo {{
        static constexpr bool reflected = false;
    }};

    /** @brief True when metadata was generated for T. */
    template <typename T>
    inline constexpr bool isReflected = StructInfo<T>::reflected;

    /** @brief Calls fn(field) for every field of T, in declaration order. */
    template <typename T, typename Fn>
    constexpr void forEachField(Fn&& fn) {{
        std::apply([&fn](auto const&... field) {{ (fn(field), ...); }}, StructInfo<T>::fields);
    }}

    /** @brief Calls fn(field, value) for every field of a concrete instance. */
    template <typename T, typename Fn>
    constexpr void forEachField(T& instance, Fn&& fn) {{
        std::apply([&](auto const&... field) {{ (fn(field, instance.*(field.pointer)), ...); }},
                   StructInfo<T>::fields);
    }}

    template <typename T, typename Fn>
    constexpr void forEachField(const T& instance, Fn&& fn) {{
        std::apply([&](auto const&... field) {{ (fn(field, instance.*(field.pointer)), ...); }},
                   StructInfo<T>::fields);
    }}

'''


def render_struct(struct, source_rel):
    qualified = "::".join(filter(None, [struct["namespace"], struct["name"]]))
    full = "::" + qualified
    out = []
    doc = struct["doc"] or ""
    out.append("    // ------------------------------------------------------------------------")
    out.append(f"    // {qualified} ({len(struct['fields'])} fields) from {source_rel}")
    out.append("    // ------------------------------------------------------------------------")
    out.append("    template <>")
    out.append(f"    struct StructInfo<{full}> {{")
    out.append(f"        using type = {full};")
    out.append("")
    out.append("        static constexpr bool reflected = true;")
    out.append(f'        static constexpr std::string_view name = "{cpp_string(struct["name"])}";')
    out.append(f'        static constexpr std::string_view qualifiedName = "{cpp_string(qualified)}";')
    out.append(f'        static constexpr std::string_view header = "{cpp_string(source_rel)}";')
    out.append(f'        static constexpr std::string_view doc = "{cpp_string(doc)}";')
    out.append("")

    if struct["fields"]:
        out.append("        static constexpr auto fields = std::make_tuple(")
        rendered = []
        for field in struct["fields"]:
            rendered.append(
                '            makeField("{name}", "{type}", "{doc}", "{default}", &type::{name})'.format(
                    name=cpp_string(field["name"]),
                    type=cpp_string(field["type"]),
                    doc=cpp_string(field["doc"]),
                    default=cpp_string(field["default"]),
                )
            )
        out.append(",\n".join(rendered))
        out.append("        );")
        out.append("")
        names = ", ".join('"%s"' % cpp_string(f["name"]) for f in struct["fields"])
        out.append(
            f"        static constexpr std::array<std::string_view, {len(struct['fields'])}> fieldNames = {{{names}}};"
        )
    else:
        out.append("        static constexpr auto fields = std::make_tuple();")
        out.append("        static constexpr std::array<std::string_view, 0> fieldNames = {};")

    out.append("        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;")
    out.append("    };")
    out.append("")
    return "\n".join(out)


def render_header(collected):
    sources = "\n".join(f"//      {rel}" for rel, _ in collected)
    seen = []
    for rel, _ in collected:
        inc = os.path.basename(rel)
        if inc not in seen:
            seen.append(inc)
    includes = "\n".join(f'#include "{inc}"' for inc in seen)

    parts = [PRELUDE.format(sources=sources, includes=includes)]
    all_types = []
    for rel, structs in collected:
        for struct in structs:
            parts.append(render_struct(struct, rel))
            all_types.append("::" + "::".join(filter(None, [struct["namespace"], struct["name"]])))

    parts.append("    /** @brief Every struct this header carries metadata for. */")
    if all_types:
        joined = ",\n        ".join(all_types)
        parts.append(f"    using ReflectedStructs = std::tuple<\n        {joined}\n    >;")
    else:
        parts.append("    using ReflectedStructs = std::tuple<>;")
    parts.append("")
    parts.append("} // namespace Andromeda::Meta")
    parts.append("")
    return "\n".join(parts)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def resolve(path):
    return path if os.path.isabs(path) else os.path.join(PROJECT_ROOT, path)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Generate struct metadata for Andromeda.")
    parser.add_argument("--input", action="append", default=None,
                        help="Header to scan (repeatable). Default: %s" % ", ".join(DEFAULT_INPUTS))
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="Destination file for the generated header.")
    parser.add_argument("--struct", action="append", default=None,
                        help="Only emit these structs (repeatable).")
    parser.add_argument("--quiet", action="store_true", help="Suppress progress output.")
    args = parser.parse_args(argv)

    inputs = args.input or DEFAULT_INPUTS
    wanted = set(args.struct) if args.struct else None

    def log(message):
        if not args.quiet:
            print(message)

    collected = []
    total_fields = 0
    for item in inputs:
        abs_in = resolve(item)
        if not os.path.isfile(abs_in):
            print(f"!! ERROR: header not found: {abs_in}", file=sys.stderr)
            return 1
        rel = os.path.relpath(abs_in, PROJECT_ROOT).replace(os.sep, "/")
        log(f"{TAG} scanning {rel}")
        structs = parse_header(abs_in)
        if wanted is not None:
            structs = [s for s in structs if s["name"] in wanted]
        for struct in structs:
            log(f"{TAG}   {struct['name']} -> {len(struct['fields'])} fields")
            total_fields += len(struct["fields"])
        collected.append((rel, structs))

    if not any(structs for _, structs in collected):
        print(f"!! ERROR: no struct found in {', '.join(inputs)}.", file=sys.stderr)
        return 1

    content = render_header(collected)
    abs_out = resolve(args.output)
    os.makedirs(os.path.dirname(abs_out), exist_ok=True)

    previous = None
    if os.path.isfile(abs_out):
        with open(abs_out, "r", encoding="utf-8") as handle:
            previous = handle.read()

    if previous == content:
        log(f"{TAG} {os.path.relpath(abs_out, PROJECT_ROOT)} is up to date ({total_fields} fields).")
        return 0

    with open(abs_out, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)
    log(f"{TAG} wrote {os.path.relpath(abs_out, PROJECT_ROOT)} ({total_fields} fields).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
