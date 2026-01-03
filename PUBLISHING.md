# Publishing pypersistent to PyPI

## Step-by-Step Guide

### 1. Setup Accounts

First, create accounts on both TestPyPI (for testing) and PyPI:

- **TestPyPI**: https://test.pypi.org/account/register/
- **PyPI**: https://pypi.org/account/register/

Enable 2FA and create API tokens for both.

### 2. Install Build Tools

```bash
pip install --upgrade build twine
```

### 3. Build Distribution Files

Build both source distribution (sdist) and wheel:

```bash
# Clean previous builds
rm -rf build/ dist/ *.egg-info

# Build source distribution and wheel
python -m build
```

This creates:
- `dist/pypersistent-1.0.0.tar.gz` - Source distribution
- `dist/pypersistent-1.0.0-cp313-cp313-macosx_*.whl` - Wheel for your platform

**Note:** The wheel is platform-specific (contains compiled C++ code).

### 4. Test on TestPyPI (IMPORTANT!)

Always test on TestPyPI first:

```bash
# Upload to TestPyPI
twine upload --repository testpypi dist/*

# Test installation from TestPyPI
pip install --index-url https://test.pypi.org/simple/ pypersistent

# Test it works
python -c "from pypersistent import PersistentMap; print(PersistentMap.create(a=1))"
```

### 5. Upload to PyPI

Once verified on TestPyPI:

```bash
# Upload to real PyPI
twine upload dist/*

# Or use API token (recommended)
twine upload -u __token__ -p pypi-YOUR_TOKEN_HERE dist/*
```

### 6. Verify Installation

```bash
pip install pypersistent
python -c "from pypersistent import PersistentMap; print(PersistentMap.create(a=1))"
```

## Important Considerations for C++ Extensions

### Platform-Specific Wheels

Your initial upload will only include a wheel for your platform (macOS ARM64). Users on other platforms will need to compile from source.

**Options:**

1. **Source-only distribution** (simplest)
   - Users compile during installation
   - Requires they have C++ compiler
   - Current approach

2. **Pre-built wheels for major platforms** (recommended)
   - Use GitHub Actions to build wheels
   - Covers Windows, macOS (Intel + ARM), Linux
   - Users get binary installation (no compiler needed)

3. **Use cibuildwheel** (best practice)
   - Automatically builds wheels for all platforms
   - Integrates with GitHub Actions
   - See example below

### Multi-Platform Build with GitHub Actions

Create `.github/workflows/wheels.yml`:

```yaml
name: Build Wheels

on:
  release:
    types: [published]

jobs:
  build_wheels:
    name: Build wheels on ${{ matrix.os }}
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-13, macos-14]

    steps:
      - uses: actions/checkout@v4

      - name: Build wheels
        uses: pypa/cibuildwheel@v2.16
        env:
          CIBW_SKIP: "pp* *-musllinux_*"
          CIBW_ARCHS_MACOS: "x86_64 arm64"

      - uses: actions/upload-artifact@v4
        with:
          name: wheels-${{ matrix.os }}
          path: ./wheelhouse/*.whl

  upload_pypi:
    needs: [build_wheels]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/download-artifact@v4
        with:
          pattern: wheels-*
          merge-multiple: true
          path: dist

      - uses: pypa/gh-action-pypi-publish@release/v1
        with:
          password: ${{ secrets.PYPI_API_TOKEN }}
```

## Version Management

For future releases:

1. Update version in `pyproject.toml` and `setup.py`
2. Create git tag: `git tag v1.0.1`
3. Push tag: `git push --tags`
4. Create GitHub release
5. CI builds and uploads wheels automatically

## Configuration Files Checklist

- ✅ `pyproject.toml` - Modern package metadata
- ✅ `setup.py` - Build configuration
- ✅ `MANIFEST.in` - Include/exclude files
- ✅ `README.md` - Package documentation
- ✅ `LICENSE` - MIT license

## Quick Reference

```bash
# Full release workflow
rm -rf build/ dist/ *.egg-info
python -m build
twine check dist/*                           # Validate
twine upload --repository testpypi dist/*    # Test
twine upload dist/*                          # Production
```

## Security Best Practices

1. **Never commit API tokens** to git
2. Use API tokens instead of passwords
3. Enable 2FA on PyPI account
4. Use GitHub Secrets for CI/CD
5. Scope tokens to specific packages

## Troubleshooting

**"File already exists"**
- You can't replace an existing version
- Bump version number and rebuild

**"Invalid distribution"**
- Run: `twine check dist/*`
- Check README renders: `python -m readme_renderer README.md -o /tmp/README.html`

**Compilation fails for users**
- Ensure `pybind11` in `install_requires`
- Document compiler requirements in README
- Consider pre-built wheels

## Resources

- PyPI Help: https://pypi.org/help/
- Packaging Guide: https://packaging.python.org/
- cibuildwheel: https://cibuildwheel.readthedocs.io/
