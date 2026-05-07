project(
    'iris',
    'cpp',
    version: '0.1.0',
    license: 'GPL-3.0-only',
    default_options: [
        'warning_level=3', 
        'cpp_std=c++20',
        'buildtype=debugoptimized'
    ],
)

inc_dir = include_directories('include')
build_inc_dir = include_directories('.') 

version_h = configure_file(
    input: 'include/Iris/Common/version.hpp.in',
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
        meson.project_source_root() / 'data' / 'errors' / 'error_codes',
        '@OUTPUT@',
    ],
    build_by_default: true,
)

lib_sources = files(
    'src/Common/error.cpp',
    'src/Debug/printer.cpp',
    'src/Frontend/lexer.cpp',
    'src/Frontend/parser.cpp',
    'src/Frontend/resolver.cpp',
    'src/Backend/compiler.cpp',
    'src/Backend/module.cpp',
    'src/Runtime/value.cpp',
    'src/Runtime/vm.cpp',
    'src/cli.cpp',
)

iris_lib = static_library(
    'iris_lib',
    [lib_sources, error_docs, version_h],
    include_directories: [inc_dir, build_inc_dir],
    install: false,
)

executable(
    'iris',
    'app/main.cpp',
    link_with: iris_lib,
    include_directories: [inc_dir, build_inc_dir],
    install: true,
    install_dir: get_option('bindir'),
)