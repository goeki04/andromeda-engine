# Andromeda tests

| Executable | What | ctest label | In CI |
|---|---|---|---|
| `AndromedaTests` | Unit tests: offline, no window, no GPU, well under a second | `unit` | required |
| `AndromedaNetworkTests` | The WebSocket client against local mock servers on 127.0.0.1 - no internet | `network` | required |
| `AndromedaGlTests` | Boots the real subsystems, needs a display (`-DANDROMEDA_BUILD_GL_TESTS=ON`) | `gl` | no |
| `AndromedaReflectionTests` | libclang reflector (`-DANDROMEDA_BUILD_REFLECTION_TESTS=ON`) | `reflection` | no |

## Running

From `andromeda/`, in a shell set up with `vcvars64.bat`:

```bat
cmake --build conan\build\Release --target AndromedaTests AndromedaNetworkTests
ctest --preset conan-release -L "unit|network" --output-on-failure
```

While working on one area, run the executable directly - it is faster and the output is shorter:

```bat
conan\build\Release\tests\AndromedaTests.exe --gtest_filter=AssetRegistry*
```

Before a larger commit, run the suite repeatedly in random order. It finds tests that depend on
each other or on timing:

```bat
conan\build\Release\tests\AndromedaTests.exe --gtest_repeat=30 --gtest_shuffle
```

## Layout

```
tests/
  unit/<module>/test_*.cpp      AndromedaTests - one folder per engine module
  network/test_*.cpp            AndromedaNetworkTests
  support/                      helpers for every test executable
    a_TempDirectory             a private folder per test, deleted afterwards
    a_LogCapture                records A_INFO/A_WARN/... to check what was reported
    a_EventManagerGuard         listeners that unregister themselves at scope end
  support/network/              helpers for the network tests
    a_TestCertificates          a CA and server certificate, generated at runtime
    a_MockWebsocketServer       TLS WebSocket server that echoes and records messages
    a_MockHttpProxy             HTTP CONNECT proxy, tunnelling or refusing
  gl/, reflection/              the opt-in suites
```

A new test is a new `.cpp` file under `unit/` or `network/`; CMake picks it up through a glob.
Header-only helpers that belong to one area live next to its tests (e.g. `unit/asset/a_AssetTestProject.hpp`).

## Rules

- **Test behaviour that can break, not arithmetic.** A test earns its place when it guards a promise
  the code makes - a Guid survives a restart, a damaged file is never overwritten, nodes run in
  dependency order - or pins a bug that was fixed. Checking that `a + b` adds is not such a test.
- **Name the behaviour.** `TEST(Suite, WhatHappens_WhenCondition)`, so a failure reads like a sentence.
  A comment above the test says *why* the behaviour matters if the name cannot.
- **Regression tests say so.** Start the comment with "Regression:" and describe what used to go wrong.
- **Never touch the real project.** No test reads or writes `assets/` or `ProjectSettings/`. Create what
  you need in a `TempDirectory`.
- **No internet, no clock.** Network tests talk to the mock servers; time comes from the code under
  test's inputs (e.g. `GraphContext::time`), not from the system clock.
- **No state leaks between tests.** The `EventManager` is a singleton - register listeners through
  `EventManagerGuard`. Swap the logger only through `LogCapture`.
- **Check the test can fail.** After writing a regression test, put the bug back for a moment and
  watch the test go red. A test that stays green either way protects nothing.
