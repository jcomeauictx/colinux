# This is not a standalone Python script, but a build declaration file
# to be read by bin/make.py. Please run bin/make.py --help.

import os
try:
    from shutil import which
except ImportError:
    which = None

from comake.settings import settings

settings.arch = os.getenv('COLINUX_ARCH')
if not settings.arch:
    settings.arch = 'i386'
    print("Target architecture not specified, defaulting to %s" % (settings.arch, ))
arch_path = pathjoin('src', 'colinux', 'arch', settings.arch)
if not os.path.isdir(arch_path):
    raise BuildCancelError('Unsupported COLINUX_ARCH=%s: missing %s' % (settings.arch, arch_path))
host_mingw_bits = os.getenv('COLINUX_HOST_MINGW_BITS')
if not host_mingw_bits:
    host_mingw_bits = '32'
if host_mingw_bits not in ('32', '64'):
    raise BuildCancelError('Unsupported COLINUX_HOST_MINGW_BITS=%s, expected 32 or 64' % host_mingw_bits)
mingw_entry_symbol = '_DriverEntry@8' if host_mingw_bits == '32' else 'DriverEntry'

current_arch_symlink = target_pathname(pathjoin('colinux', 'arch', 'current'))
if os.path.exists(current_arch_symlink):
    os.unlink(current_arch_symlink)
os.symlink(settings.arch, current_arch_symlink)

settings.host_os = os.getenv('COLINUX_HOST_OS')
if not settings.host_os:
    settings.host_os = 'winnt'
    print("Target OS not specified, defaulting to %s" % (settings.host_os, ))

current_os_symlink = target_pathname(pathjoin('colinux', 'os', 'current'))
if os.path.exists(current_os_symlink):
    os.unlink(current_os_symlink)
os.symlink(settings.host_os, current_os_symlink)

settings.cflags = os.getenv('COLINUX_CFLAGS')
if not settings.cflags:
    settings.cflags = ''

settings.lflags = os.getenv('COLINUX_LFLAGS')
if not settings.lflags:
    settings.lflags = ''

# Setup "i686-co-linux", if local gcc can't use for linux kernel
settings.gcc_guest_target = os.getenv('COLINUX_GCC_GUEST_TARGET');
cross_ddk_include = None
extra_include_paths = []
extra_lib_paths = []

compiler_defines = dict(
    COLINUX_FILE_ID='0',
    COLINUX=None,
    CO_HOST_API=None,
    COLINUX_DEBUG=None,
    COLINUX_ARCH=settings.host_os,
)
if host_mingw_bits == '64':
    compiler_defines['CO_HOST_MINGW_64'] = None
else:
    compiler_defines['CO_HOST_MINGW_32'] = None

if settings.host_os == 'winnt':
    cross_compilation_prefix = os.getenv('COLINUX_HOST_MINGW_PREFIX')
    if not cross_compilation_prefix:
        def has_tool(name):
            return which(name) if which else None
        if host_mingw_bits == '64':
            if has_tool('x86_64-w64-mingw32-gcc'):
                cross_compilation_prefix = 'x86_64-w64-mingw32-'
            elif has_tool('x86_64-pc-mingw32-gcc'):
                cross_compilation_prefix = 'x86_64-pc-mingw32-'
            elif has_tool('x86_64-w64-mingw64-gcc'):
                cross_compilation_prefix = 'x86_64-w64-mingw64-'
            else:
                print('No x86_64 mingw toolchain found, falling back to 32-bit prefix')
                cross_compilation_prefix = 'i686-w64-mingw32-'
        else:
            if has_tool('i686-w64-mingw32-gcc'):
                cross_compilation_prefix = 'i686-w64-mingw32-'
            elif has_tool('i686-pc-mingw32-gcc'):
                cross_compilation_prefix = 'i686-pc-mingw32-'
            else:
                cross_compilation_prefix = 'i686-pc-mingw32-'
    if host_mingw_bits == '64':
        compiler_flags = [
            '-m64',
            '-mpush-args',
            '-mno-accumulate-outgoing-args',
            '-std=gnu89',
        ]
    else:
        compiler_flags = [
            '-mpush-args',
            '-mno-accumulate-outgoing-args',
            '-std=gnu89',
        ]
    compiler_defines['WINVER'] = '0x0500'
    cross_gcc = None
    if which:
        cross_gcc = which('%sgcc' % cross_compilation_prefix)
    cross_ddk_include = None
    if cross_gcc:
        cross_gcc_dir = os.path.dirname(os.path.dirname(cross_gcc))
        cross_target = os.path.basename(cross_gcc).replace('-gcc', '')
        for candidate in [
            os.path.join(cross_gcc_dir, cross_target, 'include', 'ddk'),
            os.path.join(cross_gcc_dir, 'include', 'ddk'),
            os.path.join(cross_gcc_dir, 'i686-w64-mingw32', 'include', 'ddk'),
        ]:
            if os.path.isdir(candidate):
                cross_ddk_include = candidate
                break
    quasi_root = os.getenv('COLINUX_QUASIMSYS2_ROOT')
    if quasi_root:
        for item in [os.path.join(quasi_root, 'include'), os.path.join(quasi_root, 'lib')]:
            if os.path.isdir(item):
                (extra_include_paths if item.endswith('include') else extra_lib_paths).append(item)
else:
    if settings.gcc_guest_target:
        cross_compilation_prefix = settings.gcc_guest_target + '-'
    else:
        cross_compilation_prefix = ''
    compiler_flags = []

settings.target_kernel_source = getenv('COLINUX_TARGET_KERNEL_SOURCE')

if not settings.target_kernel_source:
    settings.target_kernel_source = getenv('COLINUX_TARGET_KERNEL_PATH')

if not settings.target_kernel_source:
    print("COLINUX_TARGET_KERNEL_PATH not set. Please set this environment variable to the")
    print("pathname of a coLinux-enabled kernel source tree, i.e, a Linux kernel tree that")
    print("is patched with the patch file which is under the patch/ directory.")
    raise BuildCancelError()

settings.target_kernel_build = getenv('COLINUX_TARGET_KERNEL_BUILD')

# Handle headers from in source and out of tree builds
if not settings.target_kernel_build:
    settings.target_kernel_build = settings.target_kernel_source

if settings.target_kernel_build == settings.target_kernel_source:
    settings.target_kernel_includes = [
        pathjoin(settings.target_kernel_source, 'include'),
        pathjoin(settings.target_kernel_source, 'arch/x86/include') ]
else:
    settings.target_kernel_includes = [
        pathjoin(settings.target_kernel_build, 'include'),
        pathjoin(settings.target_kernel_build, 'include2'),
        pathjoin(settings.target_kernel_source, 'arch/x86/include'),
        pathjoin(settings.target_kernel_source, 'include') ]
if cross_ddk_include:
    settings.target_kernel_includes.append(cross_ddk_include)
if extra_include_paths:
    settings.target_kernel_includes.extend(extra_include_paths)
if extra_lib_paths:
    settings.compiler_lib_paths = extra_lib_paths

    

if not hasattr(settings, 'final_build_target'):
    settings.final_build_target = 'executables'

targets['build'] = Target(
    inputs=[Input('colinux/os/%s/build/%s' % (settings.host_os,
                                              settings.final_build_target))],
    options=Options(
        overriders=dict(
            cross_compilation_prefix=cross_compilation_prefix,
        ),
        appenders=dict(
            compiler_flags=[
                '-Wno-trigraphs', '-fno-strict-aliasing', '-Wall',
                settings.cflags,
            ] + compiler_flags,
            linker_flags=[
                settings.lflags,
            ],
            compiler_lib_paths=getattr(settings, 'compiler_lib_paths', []),
            compiler_includes=[
                'src',
            ] + settings.target_kernel_includes,
            compiler_defines=compiler_defines,
        )
    ),
    tool = Empty(),
)
