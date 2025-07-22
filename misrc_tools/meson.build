project('MISRC', 'c',
        default_options: ['c_std=c11', 'buildtype=release', 'b_lto=true'],
        meson_version: '>=0.46')


warnings = [
  '-Wall',
  '-Wextra',
  '-Wshadow',
]

cflags = [
  warnings,
]

ldflags = [
]

ldflags_capture = [
]

sources_extract = [
  'misrc_extract.c',
  'extract.c',
]

sources_capture = [
  'misrc_capture.c',
  'extract.c',
  'ringbuffer.c',
]

debug_build = get_option('buildtype').startswith('debug')


host_cpu_family = host_machine.cpu_family()

host_system = host_machine.system()

if host_cpu_family == 'x86_64'
  nasm_flags = []
  if host_system == 'windows' or host_system == 'cygwin'
    if host_cpu_family == 'x86'
      cflags += '-mstackrealign'
      ldflags += '-Wl,--kill-at'
      nasm_flags += '-DPREFIX'
    endif
    nasm_flags += ['-f', 'win64']
  elif host_system == 'linux' or host_system == 'bsd' # The BSDs are close enough, right?
    if debug_build
      nasm_flags += '-gdwarf'
    endif
    nasm_flags += ['-f', 'elf64']
  elif host_system == 'darwin'
    if debug_build
      nasm_flags += '-gdwarf'
    endif
    nasm_flags += ['--prefix', '_', '-f', 'macho64']
    cflags += ['-DPREFIX']
  else
    error('Unknown host system "@0@".'.format(host_system))
  endif

  nasm_sources = [
    'extract.asm',
  ]

  nasm = find_program('nasm')

  outputname = '@BASENAME@.o'
  if host_system == 'windows'
    outputname = '@BASENAME@.obj'
  endif

  nasm_gen = generator(nasm,
                       output: outputname,
                       arguments: nasm_flags + ['@INPUT@', '-o', '@OUTPUT@'])

  nasm_genb = nasm_gen.process(nasm_sources)
  sources_extract += nasm_genb
  sources_capture +=nasm_genb
endif


deps = [ dependency('hsdaoh') ]

flac_dep =  dependency('flac', required : false)
if flac_dep.found()
  deps += [ flac_dep ]
  cflags += ['-DLIBFLAC_ENABLED=1']
  if host_system == 'windows'
    cflags += ['-DFLAC__NO_DLL=1']
  endif
  message('FLAC found, building with FLAC support')
else
  message('FLAC not found, building without FLAC support')
endif


if host_system == 'darwin'
  ldflags_capture += ['-Wl,-framework,IOKit', '-Wl,-framework,CoreFoundation', '-Wl,-framework,Security']
endif

if host_system == 'windows'
  sources_extract += [ 'getopt/getopt.c' ]
  sources_capture += [ 'getopt/getopt.c' ]
  cflags += [ '-DNTDDI_VERSION=NTDDI_WIN10_RS4', '-D_WIN32_WINNT=_WIN32_WINNT_WIN10' ]
  ldflags_capture += [ '-lonecore', '-static' ]
endif

if host_system == 'linux'
  ldflags_capture += [ '-lm' ]
endif

executable('misrc_extract',
              sources_extract,
              dependencies: [],
              link_args: ldflags,
              c_args: cflags,
              install: true)

executable('misrc_capture',
              sources_capture,
              dependencies: deps,
              link_args: ldflags + ldflags_capture,
              c_args: cflags,
              install: true)