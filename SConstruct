#!python
import os, subprocess

opts = Variables([], ARGUMENTS)

# Gets the standard flags CC, CCX, etc.
env = Environment()

#print(env.Dump())

# Define our options
opts.Add(EnumVariable('target', "Compilation target", 'release', ['d', 'debug', 'r', 'release']))
opts.Add(EnumVariable('platform', "Compilation platform", 'windows', ['', 'windows', 'x11', 'linux', 'osx']))
opts.Add(EnumVariable('p', "Compilation target, alias for 'platform'", '', ['', 'windows', 'x11', 'linux', 'osx']))
opts.Add(BoolVariable('use_llvm', "Use the LLVM / Clang compiler", 'no'))
# opts.Add(PathVariable('target_path', 'The path where the lib is installed.', 'C:/Users/seank/workspace/godot/simple-animat-world/godot/gdnative/'))
# opts.Add(PathVariable('target_name', 'The library name.', 'libagentcomm', PathVariable.PathAccept))

# Local dependency paths, adapt them to your setup
gdnative_cpp_path = "../godot-cpp/"

# Basename for the godot gdnative c++ library
gdnative_cpp_library = "libgodot-cpp"

# only support 64 at this time.. (DOES 32 bit WORK???)
env['bits'] = 64

# Updates the environment with the option variables.
opts.Update(env)

env['target_path'] = 'lib/'
env['target_name'] = 'libgodot-aibridge'

# Process some arguments
if env['use_llvm']:
    env['CC'] = 'clang'
    env['CXX'] = 'clang++'

if env['p'] != '':
    env['platform'] = env['p']

if env['platform'] == '':
    print("No valid target platform selected.")
    quit();

# Check our platform specifics
if env['platform'] == "osx":
    env['target_path'] += 'osx/'
    gdnative_cpp_library += '.osx'
    if env['target'] in ('debug', 'd'):
        env.Append(CCFLAGS = ['-g','-O2', '-arch', 'x86_64', '-std=c++17'])
        env.Append(LINKFLAGS = ['-arch', 'x86_64'])
    else:
        env.Append(CCFLAGS = ['-g','-O3', '-arch', 'x86_64', '-std=c++17'])
        env.Append(LINKFLAGS = ['-arch', 'x86_64'])

elif env['platform'] in ('x11', 'linux'):
    env['target_path'] += 'linux/'
    
    if env['bits'] == 64:
        env['target_path'] += '64/'
    else:
        raise ValueError('Requested library bits unsupported: ' + env['bits'])
        
    gdnative_cpp_library += '.linux'
    if env['target'] in ('debug', 'd'):
        env.Append(CCFLAGS = ['-fPIC', '-g3','-Og', '-std=c++17'])
    else:
        env.Append(CCFLAGS = ['-fPIC', '-g','-O3', '-std=c++17'])

# default to windows
elif env['platform'] == "windows":
    env['target_path'] += 'windows/'
    
    if env['bits'] == 64:
        env['target_path'] += '64/'
    else:
        raise ValueError('Requested library bits unsupported: ' + env['bits'])
    
    gdnative_cpp_library += '.windows'
    # This makes sure to keep the session environment variables on windows,
    # that way you can run scons in a vs 2017 prompt and it will find all the required tools
    env.Append(ENV = os.environ)

    env.Append(CCFLAGS = ['-DWIN32', '-D_WIN32', '-D_WINDOWS', '-W3', '-GR', '-D_CRT_SECURE_NO_WARNINGS'])
    if env['target'] in ('debug', 'd'):
        env.Append(CCFLAGS = ['-EHsc', '-D_DEBUG', '-MDd'])
    else:
        env.Append(CCFLAGS = ['-O2', '-EHsc', '-DNDEBUG', '-MD'])

if env['target'] in ('debug', 'd'):
    gdnative_cpp_library += '.debug'
else:
    gdnative_cpp_library += '.release'

gdnative_cpp_library += '.' + str(env['bits'])

# make sure our binding library is properly includes

CPPPATH = [
    # project headers
    './include/', 
    
    # gdnative cpp headers
    gdnative_cpp_path + 'godot_headers/', 
    gdnative_cpp_path + 'include/', 
    gdnative_cpp_path + 'include/core/', 
    gdnative_cpp_path + 'include/gen/',
    
    # ZeroMQ headers
    
    ## libsodium
    # 'C:/opt/libsodium/src/libsodium/include/', # TODO: Can I remove this?

    # ## libzmq
    'C:/opt/libzmq-src/include/', # TODO: Can I remove this?

    # ## libzmqpp
    # 'C:/opt/libzmqpp-src/src/', # TODO: Can I remove this?
    # 'C:/opt/libzmqpp/', # TODO: Can I remove this?
    
    # cppzmq
    'C:/opt/cppzmq',    

    # JSON serializer/deserializer
    'C:/opt/cpp-json/json-3.9.1/include',
       
]

LIBPATH = [

    '../godot-cpp/bin/',
    
    # ZeroMQ lib path
    
    ## libsodium
    # 'C:/opt/libsodium/bin/x64/Release/v142/dynamic/',

    ## libzmq
    'C:/opt/libzmq/bin/Release/', # 'C:\opt\libzmq\bin\Release'
    'C:/opt/libzmq/lib/Release/',

    ## libzmqpp
    'C:/opt/libzmqpp/Release/', # C:\opt\libzmqpp\Release\  
    
]

LIBS = [
    # gdnative cpp libs
    gdnative_cpp_library,
    
    # ZeroMQ libs
    
    ## libsodium
    # 'libsodium',

    ## libzmq
    'libzmq-v142-mt-4_3_5',       # dynamic lib?
    #'libzmq-v142-mt-s-4_3_5',  # static?

    ## libzmqpp
    'zmqpp',       # dynamic lib

    # 'zmqpp-static', # static lib
    
]

CPPDEFINES = [
    
]

env.Append(CPPPATH=CPPPATH)
env.Append(LIBPATH=LIBPATH)
env.Append(LIBS=LIBS)
env.Append(CPPDEFINES=CPPDEFINES)

sources = Glob('src/*.cpp')

library_binary = env['target_path'] + env['target_name']
library = env.SharedLibrary(target=library_binary, source=sources)
Default(library)

# test_sources = Glob('test/*.cpp')
# test_program = env.Program('test', source=sources + test_sources)

# Generates help for the -h scons option.
Help(opts.GenerateHelpText(env))