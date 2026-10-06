# Validation and release context: wake-source-auditor

Release version: 1.0.0, Windows x64.

Development/build validation: Windows Server 2025 x64. The release executable completed qualification on physical Windows 11 Pro 25H2 x64 (build 26200.9550) and Windows 10 Pro 22H2 x64 (build 19045.6466) in a dedicated QEMU VM. Physical Windows 10 hardware and ARM64 have not been qualified. Other platform targets in the application documentation do not imply additional tested systems.

No separately imported Microsoft Visual C++ runtime DLLs were found in the qualified executable; Windows system libraries remain required.

The final release ZIP contains the exact qualified executable. The ZIP was assembled after executable qualification; no execution of the final assembled package on Windows is claimed. SHA-256 checksums accompany the download.

Read-only describes the application's documented query behavior. Windows components can perform their own internal activity while servicing API calls. See the README and application-specific safety documentation for limits and explicit output behavior.
