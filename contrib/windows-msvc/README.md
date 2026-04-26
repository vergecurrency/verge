Windows MSVC Bootstrap
======================

This directory contains a Windows-native dependency bootstrap path for an
MSVC-based build. It is intentionally separate from `depends/`, which is
currently centered around the MinGW host path.

Goals
-----

- Keep Linux/macOS and existing MinGW workflows unchanged.
- Provide a native Windows bootstrap path suitable for Qt WebEngine.
- Pin the major Windows GUI stack to:
  - Qt `6.10.2`
  - Boost `1.90.0`
  - OpenSSL `3.6.2`

What the bootstrap script does
------------------------------

`bootstrap.ps1` will:

1. import a Visual Studio x64 developer environment
2. install Qt `6.10.2` with MSVC 2022 binaries and Qt WebEngine via `aqtinstall`
3. build Boost `1.90.0` from source with MSVC
4. build OpenSSL `3.6.2` from source with MSVC
5. build Berkeley DB `4.8.30.NC` from source with MSVC
6. bootstrap `vcpkg` and install Windows-native support libraries currently
   used by the project:
   - `libevent`
   - `protobuf`
   - `qrencode`
   - `miniupnpc`
   - `zeromq`
7. write `env-msvc.ps1` with the resulting paths and pkg-config hints

Usage
-----

From a Windows PowerShell prompt on a machine with Visual Studio Build Tools:

```powershell
pwsh -File contrib/windows-msvc/bootstrap.ps1
. .\build-msvc\env-msvc.ps1
```

The script supports skip switches for iterative work:

```powershell
pwsh -File contrib/windows-msvc/bootstrap.ps1 -SkipQt -SkipBoost
```

Current limitations
-------------------

- This is a bootstrap path, not a completed MSVC build pipeline.
- The current repo still does not have a first-class MSVC replacement for the
  existing MinGW-based `depends` host.
- If you want a full Windows WebEngine CI job, the next step is to wire a new
  workflow job to this bootstrap path and then solve the remaining MSVC-only
  project configuration issues in isolation.
