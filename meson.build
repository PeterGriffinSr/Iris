project(
    'iris',
    'cpp',
    version: '0.1.0',
    license: 'GPL-3.0-only',
    default_options: ['warning_level=3', 'cpp_std=c++20'],
)

cxx = meson.get_compiler('cpp')

version_h = configure_file(
    input: 'src/include/version.hpp.in',
    output: 'version.hpp',
    configuration: {'VCS_TAG': meson.project_version()},
)

gen_errors = find_program('scripts/gen_errors.py')
error_docs = custom_target(
    'error_docs',
    input: [],
    output: 'error_docs.hpp',
    command: [
        gen_errors,
        meson.project_source_root() / 'src' / 'errors' / 'error_codes',
        '@OUTPUT@',
    ],
    build_by_default: true,
)

sources = files(
    'src/module.cpp',
    'src/vm.cpp',
    'src/cli.cpp',
    'src/compiler.cpp',
    'src/error.cpp',
    'src/lexer.cpp',
    'src/main.cpp',
    'src/parser.cpp',
    'src/printer.cpp',
    'src/resolver.cpp',
    'src/value.cpp',
)

executable(
    'iris',
    [sources, error_docs, version_h],
    include_directories: include_directories('.'),
    install: true,
    install_dir: get_option('bindir'),
)