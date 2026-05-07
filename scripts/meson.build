gen_errors = find_program('gen_errors.py')

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