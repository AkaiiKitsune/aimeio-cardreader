compiler = meson.get_compiler('c')
hid_lib = compiler.find_library('hid')
winscard_lib = compiler.find_library('winscard')
setupapi_lib = compiler.find_library('setupapi')

deps = [
    hid_lib,
    winscard_lib,
    setupapi_lib,
]

aimeio_dll = shared_library(
    'aimeio',
    implicit_include_directories : false,    
    vs_module_defs : 'aimeio.def',
    sources: [
        'aimeio.c',
        'scard/scard.c',
    ],
    dependencies: deps,
    include_directories: [
        inc
    ]
)

executable('aimetest', 'aimetest.c', link_with : aimeio_dll)