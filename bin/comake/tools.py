import os, tempfile, shutil

class TargetNotBuildError(Exception):
    pass

class Tool(object):
    EMPTY = False

    def make(self, target, reporter):
        raise TargetNotBuildError()

    def cache(self, target):
        return ''

    def rebuild_needed(self, target):
        return False

class ToolRunInfo(object):
    def __init__(self, target, reporter, options):
        self.target = target
        self.reporter = reporter
        self.options = options 

class Empty(Tool):
    EMPTY = True
    def make(self, target, reporter):
        return 0

class Copy(Tool):
    def make(self, target, reporter):
        input_pathnames = []
        for tinput in target.get_actual_inputs():
            if tinput.get_ext() in ['.c', '.cpp']:
                actual_compile = True
            input_pathnames.append(tinput.pathname)
        if len(input_pathnames) != 1:
            raise TargetNotBuildError()

        pathnames = input_pathnames[0], target.pathname
        reporter.print_text("Copying '%s' to '%s'" % pathnames)
        shutil.copy(*pathnames)

class Executer(Tool):
    def get_cross_build_tool(self, tool, tool_run_inf):
        compiler_path = tool
        cross_compilation_prefix = tool_run_inf.options.get('cross_compilation_prefix', None)
        if cross_compilation_prefix:
            compiler_path = cross_compilation_prefix + compiler_path
        return compiler_path

    def system(self, command_line, tool_run_inf):
        tool_run_inf.reporter.print_text(command_line)
        exit_status = os.system(command_line)
        return exit_status
    
    def make(self, target, reporter):
        tool_run_inf = ToolRunInfo(target, reporter, target.options)
        self.prepare_command(tool_run_inf)
        try:
            command_line = self.get_command_line(tool_run_inf)
            exit_status = self.system(command_line, tool_run_inf)
        finally:
            self.cleanup(tool_run_inf)
        if exit_status != 0:
            raise TargetNotBuildError()

    def cache(self, target):
        tool_run_inf = ToolRunInfo(target, None, target.options)
        return self.get_command_line(tool_run_inf)    

    def prepare_command(self, tool_run_inf):
        pass

    def get_command_line(self, tool_run_inf):
        return ''

    def cleanup(self, tool_run_inf):
        pass

class Compiler(Executer):
    def get_command_line(self, tool_run_inf):
        actual_compile = False
        compiler_def_type = tool_run_inf.options.get('compiler_def_type', [])
        if not compiler_def_type:
            compiler_def_type = 'gcc'
        compiler_path = self.get_cross_build_tool(compiler_def_type, tool_run_inf)
        
        input_pathnames = []
        for tinput in tool_run_inf.target.get_actual_inputs():
            if tinput.get_ext() in ['.c', '.cpp']:
                actual_compile = True
            input_pathnames.append(tinput.pathname)

        parameters = []
        parameters.append(compiler_path)
        if actual_compile:
            compiler_includes = tool_run_inf.options.get('compiler_includes', [])
            parameters += ["-I" + path for path in compiler_includes]
            compiler_flags = tool_run_inf.options.get('compiler_flags', [])
            parameters += compiler_flags
            compiler_optimization = tool_run_inf.options.get('compiler_optimization', "-O2")
            parameters += [compiler_optimization]
            compiler_defines = tool_run_inf.options.get('compiler_defines', {})
            defines = compiler_defines.items()
            defines.sort()
            for key, value in defines:
                if value is not None:
                    parameters.append('-D%s=%s' % (key, value))
                    continue
                parameters.append('-D%s' % (key, ))
            parameters.append('-c')
        else:
            linker_flags = tool_run_inf.options.get('linker_flags', [])
            parameters += linker_flags

        parameters.extend(input_pathnames)
        compiler_lib_paths = tool_run_inf.options.get('compiler_lib_paths', [])
        compiler_libs = tool_run_inf.options.get('compiler_libs', [])
        parameters += ["-L" + path for path in compiler_lib_paths]
        parameters += ["-l" + path for path in compiler_libs]
        
        parameters.append('-o')
        parameters.append(tool_run_inf.target.pathname)

        compiler_strip = tool_run_inf.options.get('compiler_strip', False)
        if compiler_strip:
            parameters.append('&&')
            parameters.append(self.get_cross_build_tool("strip --strip-debug", tool_run_inf))
            parameters.append(tool_run_inf.target.pathname)

        command_line = ' '.join(parameters)
        return command_line

class Linker(Executer):
    def get_command_line(self, tool_run_inf):
        actual_compile = False
        
        link_path = self.get_cross_build_tool('ld', tool_run_inf)

        input_pathnames = []
        for tinput in tool_run_inf.target.get_actual_inputs():
            input_pathnames.append(tinput.pathname)

        parameters = []
        parameters.append(link_path)
        if tool_run_inf.target.get_ext() == '.o':
            parameters.append('-r')
            
        parameters.extend(input_pathnames)   
        parameters.append('-o')
        parameters.append(tool_run_inf.target.pathname)
        command_line = ' '.join(parameters)
        return command_line

class Archiver(Executer):
    def __init__(self, flat=True):
        self.flat = flat
        
    def get_command_line(self, tool_run_inf):
        ar = self.get_cross_build_tool('ar', tool_run_inf)
        parameters = []
        parameters.append(ar)
        parameters.append('cr')
        parameters.append(tool_run_inf.target.pathname)
        parameters.extend(tool_run_inf.ready_inputs)
        command_line = ' '.join(parameters)
        return command_line

    def cache(self, target):         
        tool_run_inf = ToolRunInfo(target, None, target.options)
        cmdline = self.get_cross_build_tool('ar', tool_run_inf)
        cmdline += ' '.join([target.pathname for target in tool_run_inf.target.get_actual_inputs()])
        return cmdline

    def prepare_command(self, tool_run_inf):
        tool_run_inf.flat_mode = False
        ready_inputs = []
        tool_run_inf.ready_inputs = ready_inputs
        tool_run_inf.temp_dir = None
        tool_run_inf.temp_dir_extract = None
        tool_run_inf.temp_inputs = []
        
        ar = self.get_cross_build_tool('ar', tool_run_inf)
        
        input_pathnames = []
        for tinput in tool_run_inf.target.get_actual_inputs():
            if tinput.get_ext() == '.a':
                if self.flat:
                    tool_run_inf.flat_mode = True
                else:
                    ready_inputs.append(tinput.pathname)
            else:
                ready_inputs.append(tinput.pathname)

        if tool_run_inf.flat_mode:
            temp_dir = tempfile.mkdtemp()
            temp_dir_extract = tempfile.mkdtemp()
            tool_run_inf.temp_dir = temp_dir
            tool_run_inf.temp_dir_extract = temp_dir_extract
            file_index = 0
            
            for tinput in tool_run_inf.target.get_actual_inputs():
                if tinput.get_ext() == '.a':
                    cwd = os.getcwd()
                    real_path = os.path.realpath(tinput.pathname)
                    try:
                        os.chdir(temp_dir_extract)
                        self.system('ar x %s' % (real_path, ), tool_run_inf)
                    finally:
                        os.chdir(cwd)
                        
                    for pathname in os.listdir(temp_dir_extract):
                        fullname = os.path.join(temp_dir_extract, pathname)
                        destname = os.path.join(temp_dir, ("%d-" % (file_index, )) + pathname)
                        os.rename(fullname, destname)
                        tool_run_inf.ready_inputs.append(destname)
                        tool_run_inf.temp_inputs.append(destname)
                        file_index += 1

    def cleanup(self, tool_run_inf):
        if tool_run_inf.temp_dir:
            for pathname in tool_run_inf.temp_inputs:
                os.unlink(pathname)
            os.rmdir(tool_run_inf.temp_dir)
            os.rmdir(tool_run_inf.temp_dir_extract)

class Script(Executer):
    def __init__(self, func):
        self.func = func

    def get_command_line(self, tool_run_inf):
        return self.func(self, tool_run_inf)

exported_tools = [
    Compiler,
    Linker,
    Empty,
    Script,
    Tool,
    Executer,
    Copy,
]
