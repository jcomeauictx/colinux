def script_cmdline(scripter, tool_run_inf):
    inputs = tool_run_inf.target.get_actual_inputs()
    command_line = ((
        "%s -O coff -I %s %s -o %s"
        ) %
    (scripter.get_cross_build_tool('windres', tool_run_inf),
     current_dirname,
     inputs[0].pathname,
     tool_run_inf.target.pathname,))
    return command_line

targets['daemon.res'] = Target(
    tool = Script(script_cmdline),
    inputs = [
       Input('daemon.rc'),
       Input('resources_def.inc'),
    ],
)

targets['colinux.res'] = Target(
    tool = Script(script_cmdline),
    inputs = [
       Input('colinux.rc'),
       Input('resources_def.inc'),
    ],
)

def script_cmdline(scripter, tool_run_inf):
    command_line = '%s %s %s' % (
        tool_run_inf.target.inputs[0].pathname,
        tool_run_inf.target.inputs[1].pathname,
        tool_run_inf.target.pathname)
    return command_line

targets['resources_def.inc'] = Target(
    tool = Script(script_cmdline),
    inputs = [
       Input('resources_def.sh'),
       Input('src/colinux/VERSION', root_relative=True),
       Input('bin/build-common.sh', root_relative=True),
    ]
)
