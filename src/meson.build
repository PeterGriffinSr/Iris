lib_sources = files(
    'Backend/compiler.cpp',
    'Backend/module.cpp',
    'Common/error.cpp',
    'Debug/printer.cpp',
    'Frontend/lexer.cpp',
    'Frontend/parser.cpp',
    'Frontend/resolver.cpp',
    'Frontend/scheme.cpp',
    'Frontend/type.cpp',
    'Frontend/type_env.cpp',
    'Frontend/typechecker.cpp',
    'Frontend/unifier.cpp',
    'Runtime/value.cpp',
    'Runtime/vm.cpp',
    'cli.cpp',
)

iris_lib = static_library(
    'iris_lib',
    [lib_sources, error_docs, version_h],
    include_directories: [inc_dir, build_inc_dir],
    install: false,
)

iris_dep = declare_dependency(link_with: iris_lib, include_directories: [inc_dir, build_inc_dir])