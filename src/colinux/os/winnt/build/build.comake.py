targets['executables'] = Target(
    inputs=[
    Input('colinux-daemon.exe'),
    Input('colinux-net-daemon.exe'),
    Input('colinux-console-fltk.exe'),
    Input('colinux-debug-daemon.exe'),
    Input('colinux-console-nt.exe'),
    Input('colinux-bridged-net-daemon.exe'),
    Input('colinux-slirp-net-daemon.exe'),
    Input('colinux-serial-daemon.exe'),
    Input('linux.sys'),
    ],
    tool = Empty(),
)

def generate_options(compiler_def_type, libs=None, lflags=None):
    if not libs:
        libs = []
    if not lflags:
        lflags = []
    return Options(
        overriders = dict(
            compiler_def_type = compiler_def_type,
            compiler_strip = True,
        ),
        appenders = dict(
        compiler_flags = [ '-mno-cygwin' ],
	linker_flags = lflags,
        compiler_libs = libs + [
            'user32', 'gdi32', 'ws2_32', 'ntdll', 'kernel32', 'ole32', 'uuid', 'gdi32',
            'msvcrt', 'crtdll', 'shlwapi', 
        ]),
    )

user_dep = [Input('../user/user-all.a')]
user_res = [Input('../user/daemon/res/daemon.res')]

targets['colinux-daemon.exe'] = Target(
    inputs = user_res + [
        Input('../user/daemon/daemon.o'),
    ] + user_dep,
    tool = Compiler(),
    mono_options = generate_options('gcc'),
)

targets['colinux-net-daemon.exe'] = Target(
    inputs = user_res + [
       Input('../user/conet-daemon/build.o'),
       Input('../../../user/daemon-base/build.o'),
    ] + user_dep,
    tool = Compiler(),    
    mono_options = generate_options('g++'),
)

targets['colinux-bridged-net-daemon.exe'] = Target(
    inputs = user_res + [
       Input('../user/conet-bridged-daemon/build.o'),
       Input('../../../user/daemon-base/build.o'),
    ] + user_dep,
    tool = Compiler(),    
    mono_options = generate_options('g++', libs=['wpcap']),
)

targets['colinux-slirp-net-daemon.exe'] = Target(
    inputs = user_res + [
       Input('../user/conet-slirp-daemon/build.o'),
       Input('../../../user/slirp/build.o'),
       Input('../../../user/daemon-base/build.o'),
    ] + user_dep,
    tool = Compiler(),    
    mono_options = generate_options('g++', libs=['iphlpapi']),
)

targets['colinux-serial-daemon.exe'] = Target(
    inputs = user_res + [
       Input('../user/coserial-daemon/build.o'),
       Input('../../../user/daemon-base/build.o'),
    ] + user_dep,
    tool = Compiler(),    
    mono_options = generate_options('g++'),
)

targets['colinux-console-fltk.exe'] = Target(
    inputs = user_res + [
       Input('../user/console/build.o'),
       Input('../../../user/console/build.o'),
    ] + user_dep,
    tool = Compiler(),    
    mono_options = generate_options('g++', libs=['fltk'], lflags=['-mwindows']),
)

targets['colinux-console-nt.exe'] = Target(
    inputs = user_res + [
       Input('../user/console-nt/build.o'),
       Input('../../../user/console-base/build.o'),
    ] + user_dep,
    tool = Compiler(),    
    mono_options = generate_options('g++'),
)

targets['colinux-debug-daemon.exe'] = Target(
    inputs = user_res + [
       Input('../user/debug/build.o'),
       Input('../../../user/debug/build.o'),
    ] + user_dep,
    tool = Compiler(),    
    mono_options = generate_options('g++'),
)

targets['driver.o'] = Target(
    inputs = [
       Input('../../../kernel/build.o'),
       Input('../kernel/build.o'),
       Input('../../../arch/build.o'),
       Input('../../../common/common.a'),
    ],
    tool = Linker(),
)

def script_cmdline(scripter, tool_run_inf):
    inputs = tool_run_inf.target.get_actual_inputs()
    command_line = ((
        "%s "
        "-Wl,--subsystem,native "
        "-Wl,--image-base,0x10000 "
        "-Wl,--file-alignment,0x1000 "
        "-Wl,--section-alignment,0x1000 "
        "-Wl,--entry,_DriverEntry@8 "
        "-Wl,%s "
        "-mdll -nostartfiles -nostdlib "
        "-o %s %s -lntoskrnl -lhal -lgcc ") %
    (scripter.get_cross_build_tool('gcc', tool_run_inf),
     inputs[1].pathname,
     tool_run_inf.target.pathname,
     inputs[0].pathname))
    return command_line

targets['linux.sys'] = Target(
    tool = Script(script_cmdline),
    inputs = [
       Input('driver.o'),
       Input('driver.base.exp'),
    ],
    options = Options(
        overriders = dict(
            compiler_strip = True,
        ),
        appenders = dict(
            compiler_defines = dict(
                __KERNEL__=None,
                CO_KERNEL=None,
                CO_HOST_KERNEL=None,
            ),
        )
    )
)

def script_cmdline(scripter, tool_run_inf):
    inputs = tool_run_inf.target.get_actual_inputs()
    command_line = ((
        "%s "
        "--dllname linux.sys  "
        "--base-file %s "
        "--output-exp %s") %
    (scripter.get_cross_build_tool('dlltool', tool_run_inf),
     inputs[0].pathname,
     tool_run_inf.target.pathname))
    return command_line


targets['driver.base.exp'] = Target(
    tool = Script(script_cmdline),
    inputs = [
       Input('driver.base.tmp'),
    ],
)

def script_cmdline(scripter, tool_run_inf):
    inputs = tool_run_inf.target.get_actual_inputs()
    command_line = ((
        "%s "
        "-Wl,--base-file,%s " 
	"-Wl,--entry,_DriverEntry@8 "
	"-nostartfiles -nostdlib "
        "-o junk.tmp %s -lntoskrnl -lhal -lgcc ; "
	"rm -f junk.tmp") %
    (scripter.get_cross_build_tool('gcc', tool_run_inf),
     tool_run_inf.target.pathname,
     inputs[0].pathname))
    return command_line


targets['driver.base.tmp'] = Target(
    tool = Script(script_cmdline),
    inputs = [
       Input('driver.o'),
    ],
)

targets['installer'] = Target(
    inputs = [
       Input('executables'),
       Input('../user/install/coLinux.exe'),
    ],
    tool = Empty(),
)

