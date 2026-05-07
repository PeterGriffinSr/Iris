project(
    'iris',
    'cpp',
    version: '0.1.0',
    license: 'GPL-3.0-only',
    default_options: ['warning_level=3', 'cpp_std=c++20', 'buildtype=release'],
)

inc_dir = include_directories('include')
build_inc_dir = include_directories('.')

subdir('scripts')
subdir('include/Iris/Common')

subdir('src')

subdir('app')