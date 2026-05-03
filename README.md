# Iris

## Installation

### Prerequisites

- C++ compiler (GCC, Clang, or MSVC)
- Python 3.6+

### Build Steps

1. Install build dependencies:
    ```bash
    pip install meson ninja
    ```
2. Set up the build directory:
    ```bash
    meson setup build --buildtype=release
    ```
3. Compile the project:
    ```bash
    meson compile -C build
    ```
4. Install:
    ```bash
    meson install
    ```

## Usage

Once installed, you can run with:
```bash
iris [script.<is/iris>]
```

## Development

To build in debug mode for development:
```bash
meson setup build --buildtype=debug
meson compile -C build
```

## License

Iris is licensed under the GNU-GPL 3.0 Only