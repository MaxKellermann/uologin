project('uologin', 'cpp', version: '0.5',
  meson_version: '>= 1.2',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',

    'fmt:cpp_std=c++23',
  ],
)

debug = get_option('b_ndebug') != 'true'

compiler = meson.get_compiler('cpp')

conf = configuration_data()

common_flags = [
  '-D_GNU_SOURCE',
]

test_common_flags = [
  '-Wcast-align',
  '-Wcast-qual',
  '-Wdouble-promotion',
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wmissing-format-attribute',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wundef',
  '-Wunused',
  '-Wvla',
  '-Wwrite-strings',

  '-Wno-missing-field-initializers',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  '-Wused-but-marked-unused',
]

test_global_common_flags = [
  '-fvisibility=hidden',
]

add_global_link_arguments(
  compiler.get_supported_link_arguments(
    # make relocations read-only (hardening)
    '-Wl,-z,relro',

    # no lazy binding, please - not worth it for a daemon
    '-Wl,-z,now',
  ),
  language: 'cpp'
)

if not debug
  test_global_common_flags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]

  add_global_link_arguments(
    compiler.get_supported_link_arguments(
      '-Wl,--gc-sections',
      '-Wl,--icf=all',
    ),
    language: 'cpp'
  )
endif

test_global_cxxflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

libsystemd = dependency('libsystemd', required: get_option('systemd'))
libsodium = dependency('libsodium')
berkeleydb = compiler.find_library('db')

inc = include_directories(
  '.',
  'src',
  'libcommon/src',
)

libcommon_enable_DefaultFifoBuffer = false

subdir('libcommon/src/util')
subdir('libcommon/src/lib/fmt')
subdir('libcommon/src/io')
subdir('libcommon/src/io/config')
subdir('libcommon/src/system')
subdir('libcommon/src/net')
subdir('libcommon/src/event')
subdir('libcommon/src/event/net')
subdir('libcommon/src/stock')

if libsystemd.found()
  subdir('libcommon/src/event/systemd')
  libsystemd = event_systemd_dep
endif

conf.set('HAVE_LIBSYSTEMD', libsystemd.found())
configure_file(output: 'config.h', configuration: conf)

executable(
  'uologin',
  'src/Main.cxx',
  'src/CommandLine.cxx',
  'src/Config.cxx',
  'src/BerkeleyDB.cxx',
  'src/Database.cxx',
  'src/Instance.cxx',
  'src/Listener.cxx',
  'src/KnockListener.cxx',
  'src/Connection.cxx',
  'src/DelayedConnection.cxx',
  'src/PipeStock.cxx',
  'src/Splice.cxx',
  'src/Nftables.cxx',
  'src/net/AccountedClientConnection.cxx',
  'src/net/ClientAccounting.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    libsodium,
    berkeleydb,
    util_dep,
    io_config_dep,
    net_dep,
    event_net_dep,
    stock_dep,
  ],
  install: true,
)

install_data('uologin.conf', install_dir: get_option('sysconfdir'))
