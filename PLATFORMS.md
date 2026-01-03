# Platform Support

pypersistent builds wheels for all major platforms through GitHub Actions.

## Supported Platforms

### **Operating Systems**
- ✅ **Linux** (x86_64, ARM64/aarch64)
- ✅ **macOS** (Intel x86_64, Apple Silicon ARM64)
- ✅ **Windows** (x86_64)

### **Python Versions**
- ✅ Python 3.10
- ✅ Python 3.11
- ✅ Python 3.12
- ✅ Python 3.13
- ✅ Python 3.14

### **Wheel Matrix**

GitHub Actions builds **40+ wheels** covering:

| OS | Architecture | Python Versions |
|----|--------------|-----------------|
| Linux | x86_64 | 3.10, 3.11, 3.12, 3.13, 3.14 |
| Linux | aarch64 (ARM64) | 3.10, 3.11, 3.12, 3.13, 3.14 |
| macOS | x86_64 (Intel) | 3.10, 3.11, 3.12, 3.13, 3.14 |
| macOS | arm64 (Apple Silicon) | 3.10, 3.11, 3.12, 3.13, 3.14 |
| Windows | AMD64 (x86_64) | 3.10, 3.11, 3.12, 3.13, 3.14 |

### **Installation**

Users get pre-built wheels for their platform:

```bash
pip install pypersistent
```

**Fast installation** (no compilation required) on:
- Ubuntu/Debian Linux (x86_64 and ARM64)
- macOS (Intel and M1/M2/M3)
- Windows (64-bit)

## Build System

- **Tool**: [cibuildwheel](https://cibuildwheel.readthedocs.io/)
- **CI**: GitHub Actions
- **Cost**: **FREE** (public repository)
- **Trigger**: Automatic on release, or manual via workflow_dispatch

## Testing

Each wheel is tested during build:
- Import test
- Basic functionality test
- Full test suite on PR/push

## Not Supported

- ❌ PyPy (uses different C API)
- ❌ musllinux/Alpine Linux (can build from source)
- ❌ 32-bit systems (rare in 2024+)
- ❌ Python < 3.10 (Python 3.8 EOL June 2023, Python 3.9 EOL October 2025)

## Fallback: Source Distribution

If no wheel matches your platform:
- Installs from source distribution (.tar.gz)
- Requires C++17 compiler
- Automatically compiled during `pip install`

## GitHub Actions Status

Check build status:
- Latest builds: https://github.com/cmarschner/pypersistent/actions
- Release workflow: `.github/workflows/build-wheels.yml`
- Test workflow: `.github/workflows/tests.yml`

## Build Time

Typical build times on GitHub Actions:
- Linux wheels: ~15 minutes
- macOS wheels: ~20 minutes
- Windows wheels: ~25 minutes
- **Total**: ~30-40 minutes (parallel execution)

All builds run in parallel at **no cost** for public repositories!

## GitHub Runner Images

- **Linux**: `ubuntu-latest` (x86_64 + aarch64 via QEMU)
- **Windows**: `windows-latest` (AMD64)
- **macOS Intel**: `macos-15-intel` (x86_64)
- **macOS ARM**: `macos-latest` (Apple Silicon ARM64)
