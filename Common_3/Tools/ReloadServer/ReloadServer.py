# Copyright (c) 2017-2024 The Forge Interactive Inc.
#
# This file is part of The-Forge
# (see https://github.com/ConfettiFX/The-Forge).
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import argparse
import json
import hashlib
import logging
import os
import platform
import socket
import struct
import subprocess
import sys
import traceback
import time
from pathlib import Path
from typing import List, Dict, Union
from concurrent.futures import ThreadPoolExecutor, as_completed

DEFAULT_PORT = 6543
MAX_CLIENT_MESSAGE_LENGTH = 1024 + 64  # FS_MAX_PATH + ShaderPlatformMaxLength
LOG_PREFIX = "ReloadServer [HOST]"
CURRENT_SCRIPT = Path(__file__).absolute()
FSL = CURRENT_SCRIPT.parent.parent / 'ForgeShadingLanguage' / 'fsl.py'
GLOBAL_MUTEX_NAME = 'ReloadServerGlobalMutex'
LOGGER = None


if os.name == 'nt':
    import ctypes
    from ctypes import wintypes

    # Create ctypes wrapper for Win32 functions we need, with correct argument/return types
    _CreateMutex = ctypes.windll.kernel32.CreateMutexW
    _CreateMutex.argtypes = [wintypes.LPCVOID, wintypes.BOOL, wintypes.LPCWSTR]
    _CreateMutex.restype = wintypes.HANDLE

    _WaitForSingleObject = ctypes.windll.kernel32.WaitForSingleObject
    _WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    _WaitForSingleObject.restype = wintypes.DWORD

    _ReleaseMutex = ctypes.windll.kernel32.ReleaseMutex
    _ReleaseMutex.argtypes = [wintypes.HANDLE]
    _ReleaseMutex.restype = wintypes.BOOL

    _CloseHandle = ctypes.windll.kernel32.CloseHandle
    _CloseHandle.argtypes = [wintypes.HANDLE]
    _CloseHandle.restype = wintypes.BOOL


    class GlobalMutex(object):
        def __init__(self, name: str):
            self.name = name
            ret = _CreateMutex(None, False, f'Global{name}')
            if not ret:
                raise ctypes.WinError()
            self.handle = ret

        def acquire(self):
            ret = _WaitForSingleObject(self.handle, 0)
            if ret in (0, 0x80):
                # Note that this doesn't distinguish between normally acquired (0) and
                # acquired due to another owning process terminating without releasing (0x80)
                return True
            elif ret == 0x102:
                # Timeout
                return False
            else:
                raise ctypes.WinError()

        def release(self):
            ret = _ReleaseMutex(self.handle)
            if not ret:
                raise ctypes.WinError()

        def close(self):
            ret = _CloseHandle(self.handle)
            if not ret:
                raise ctypes.WinError()

else:
    import fcntl
    import errno

    class GlobalMutex(object):
        def __init__(self, name: str):
            self.fd = os.open(f'/tmp/{name}', os.O_CREAT)

        def acquire(self):
            try:
                fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return True
            except OSError as e:
                code = e.args[0]
                if code != errno.EWOULDBLOCK:
                    raise e
                return False

        def release(self):
            fcntl.flock(self.fd, fcntl.LOCK_UN)

        def close(self):
            os.close(self.fd)


class CustomLogger(object):
    def __init__(self, file_path: Path):
        self.file_path = file_path

    def info(self, msg):
        self._emit(logging.INFO, msg)

    def error(self, msg):
        self._emit(logging.ERROR, msg)

    def _emit(self, level, msg):
        now = time.time()
        dec = f'{now - int(now):.6f}'
        t = time.strftime('%d-%m-%Y %H:%M:%S', time.gmtime(now)) + dec[1:]
        with open(self.file_path, 'a') as f:
            f.write(f'{t} [{logging.getLevelName(level)}] {msg}\n')


class Msg:
    SHADER = b's'
    RECOMPILE = b'r'
    KILL_SERVER = b'k'
    SUCCESS = b'\x00'
    ERROR = b'\x01'


class MissingFslCmdsDirException(BaseException):
    pass


class Shader(object):
    relative_path: str
    bytecode: bytes

    def __init__(self, relative_path: str, bytecode: bytes):
        self.relative_path = relative_path
        self.bytecode = bytecode


def get_host_ip():
    ips = None
    try:
        ips = socket.gethostbyname_ex(socket.gethostname())[2]
        for ip in sorted(ips):
            if ip.startswith('192.168'):
                return ip
    except:
        pass
    print(f'Could not find local network IP in the list of all IP addresses: {ips}')
    return None


def generate_reload_server_info(fsl_input, binary_dir, intermediate_dir, port, args_to_cache):
    # write arguments to JSON file
    fsl_cmds = os.path.join(intermediate_dir, 'fsl_cmds')
    os.makedirs(fsl_cmds, exist_ok=True)
    file_name = str(hashlib.md5(fsl_input.encode()).hexdigest()) + '.json'
    with open(os.path.join(fsl_cmds, file_name), 'w') as f:
        json.dump(args_to_cache, f)

    # write info to text file
    mutex = GlobalMutex(f'{hashlib.md5(str(binary_dir).encode()).hexdigest()}')
    if not mutex.acquire():
        return
    try:
        reload_server_file = os.path.join(binary_dir, 'reload-server.txt')
        with open(reload_server_file, 'w') as f:
            lines = [
                get_host_ip() or '127.0.0.1',
                str(port),
                intermediate_dir
            ]
            for line in lines:
                f.write(line)
                f.write('\n')
    except:
        raise
    finally:
        mutex.release()


def kwargs_ensure_no_popup_console() -> dict:    
    # NOTE: To avoid creating popup consoles when compiling shaders
    # on Windows, we pass shell=True here explicitly. However,
    # on macOS/Linux this causes hangs so we only do it for Windows.
    is_win32 = platform.system() == 'Windows'
    return {'shell': is_win32}


def get_file_times_recursive(root: Path) -> Dict[Path, float]:
    # Skip checking text files and shader sources
    suffixes_to_ignore = [
        '.txt', '.json', '.fsl', '.hlsl', '.glsl',
        '.vert', '.frag', '.tesc', '.tese', '.geom', '.comp',
    ]
    file_times = {}
    for path in root.rglob('*'):
        if not path.is_dir() and path.suffix not in suffixes_to_ignore:
            file_times[path] = path.stat().st_mtime
    return file_times


def get_fsl_arg_index(cmd: list, *args: str) -> int:
    for arg in args:
        if arg in cmd:
            return cmd.index(arg)
    return -1


def decode_u32_le(n: bytes) -> int:
    return struct.unpack('<I', n)[0]


def decode_strings_from_bytes(count: int, data: bytes) -> List[str]:
    offset = 0
    strings = []
    for _ in range(0, count):
        length = decode_u32_le(data[offset:offset+4])
        strings.append(data[offset+4:offset+4+length].decode())
        offset += length + 4 + 1
    return strings


def encode_u32_le(n: int) -> bytes:
    return struct.pack('<I', n)


def encode_shaders_to_bytes(shaders: List[Shader]) -> bytearray:
    output = bytearray(Msg.SHADER)
    output.extend(encode_u32_le(0))  # total size, set later
    output.extend(encode_u32_le(len(shaders)))
    for shader in shaders:
        output.extend(encode_u32_le(len(shader.relative_path)))
        output.extend(encode_u32_le(len(shader.bytecode)))
        output.extend(shader.relative_path.encode('utf8'))
        output.append(0) # null termination for path string
        output.extend(shader.bytecode)
    output[1:5] = encode_u32_le(len(output) - 5)
    return output


def read_shaders_from_disk(paths: List[str], root: Path) -> List[Shader]:
    result = []
    with ThreadPoolExecutor() as pool:
        futures = {pool.submit(lambda x: (root / x).read_bytes(), p): p for p in paths}
        for future in as_completed(futures):
            path = futures[future]
            try:
                data = future.result()
                result.append(Shader(str(path), data))
            except Exception:
                raise RuntimeError(f'Failed to read shader at non-exisent path `{path}`, binaryDestination: `{root}`',)
    return result


def recompile_shaders(intermediate_root: Path, platform: str) -> Union[str, List[Shader]]:
    cmds = []    
    fsl_cmds_dir = intermediate_root / 'fsl_cmds'
    if not fsl_cmds_dir.exists():
        raise MissingFslCmdsDirException()

    for cmd_path in fsl_cmds_dir.iterdir():
        if cmd_path.is_dir():
            continue
        with open(cmd_path) as f:
            cmd = json.load(f)
            # replace language argument with currently selected API
            lang_index = get_fsl_arg_index(cmd, '-l', '--language')
            if lang_index != -1:
                cmd[lang_index + 1] = f'{platform}'
            # remove `--cache-args` from list
            cache_args_index = cmd.index('--cache-args')
            if cache_args_index != -1:
                cmd = cmd[:cache_args_index] + cmd[cache_args_index + 1:]
            cmds.append(cmd)

    if not cmds:
        return []

    # To avoid requiring knowledge about `which` shaders are being compiled,
    # we collect ALL file times in the `shaders_root` directory before and after re-compilation
    # and upload every shader that has been modified by the call to `recompile_shaders`.
    shaders_root = Path(cmds[0][get_fsl_arg_index(cmds[0], '-b', '--binaryDestination') + 1])
    file_times_before = get_file_times_recursive(shaders_root)

    procs = []
    extra_kwargs = kwargs_ensure_no_popup_console()
    for cmd in cmds:
        full_cmd = [sys.executable, str(FSL)] + cmd
        procs.append(subprocess.Popen(
            full_cmd, 
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            **extra_kwargs
        ))
    
    ret = 0
    output = bytearray()
    for proc in procs:
        (stdout, stderr) = proc.communicate()
        if not ret and proc.returncode:
            ret = proc.returncode
        # TODO: Do we want to format stdout/stderr or just concatenate them?
        output.extend(stdout)
        output.extend(stderr)

    output = output.decode().strip()
    if output:
        log_info(f'fsl.py output:\n{output}')
    if ret != 0: 
        return output

    file_times_after = get_file_times_recursive(shaders_root)
    updated_shader_paths = []
    for path, modified_time in file_times_before.items():
        if path in file_times_after and modified_time < file_times_after[path]:
            updated_shader_paths.append(str(path.relative_to(shaders_root).as_posix()))

    return read_shaders_from_disk(updated_shader_paths, shaders_root)


def setup_logging(file_name: str = 'server-log.txt'):
    global LOGGER
    LOGGER = CustomLogger(CURRENT_SCRIPT.parent / file_name)


def log_info(error: str):
    print(f'{LOG_PREFIX}: {error}')
    if LOGGER is not None:
        LOGGER.info(f'{LOG_PREFIX}: {error}')


def log_error(error: str):
    print(f'{LOG_PREFIX}: {error}', file=sys.stderr)
    if LOGGER is not None:
        LOGGER.error(f'{LOG_PREFIX}: {error}')


def send_error_and_close(client: socket.socket, error: str):
    log_error(error)

    msg = bytearray(Msg.ERROR)
    msg.extend(encode_u32_le(0)) # total size, set later
    msg.extend(b'Shader recompilation returned following errors:\n')
    msg.extend(error.encode())
    msg.append(0)
    msg[1:5] = encode_u32_le(len(msg) - 5)
    client.sendall(msg)
    client.close()


def server_loop(server: socket.socket) -> bool:
    try:
        (conn, _) = server.accept()
    except (TimeoutError, socket.timeout):
        return False

    conn.settimeout(0.5)
    try:
        data = conn.recv(MAX_CLIENT_MESSAGE_LENGTH)

        # Checking server status succeeds if a connection is made, so we have nothing to do
        if data and data[:1] == Msg.KILL_SERVER:
            conn.close()
            return True
        
        # We only allow the client to sent RECOMPILE messages that contain the shader binary root
        if not data or data[:1] != Msg.RECOMPILE:
            send_error_and_close(conn, f'Expected RECOMPILE message `{Msg.RECOMPILE}`, got `{data[0]}` instead')
            return False

        # Get intermediate directory and platform that will be used for shader recompilation
        platform, intermediate_root = decode_strings_from_bytes(2, data[1:])
        intermediate_root = Path(intermediate_root)
        if not intermediate_root.exists():
            send_error_and_close(conn, f'Path `{str(intermediate_root)}` does not exist')
            return False
        
    except (TimeoutError, socket.timeout):
        send_error_and_close(conn, 'Timed out while communicating with client - please try again')
        return False
    
    result_or_error = recompile_shaders(intermediate_root, platform)
    if isinstance(result_or_error, str):
        send_error_and_close(conn, result_or_error)
        return False
    
    encoded_shaders = encode_shaders_to_bytes(result_or_error)
    log_info(f'Uploading {len(result_or_error)} shaders to client ({len(encoded_shaders)} bytes)')

    conn.sendall(encoded_shaders)
    conn.close()
    return False


def send_message(msg: bytes, port: int, timeout: float = 0.1) -> bool:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect(('localhost', port))
        sock.send(msg)
        return True
    except:
        return False


def enable_adb_port_reversal(port: int):
    try:
        subprocess.call(
            ['adb', 'reverse', f'tcp:{port}', f'tcp:{port}'], 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE,
            **kwargs_ensure_no_popup_console()
        )
    except FileNotFoundError:
        return
    except:        
        log_error(traceback.format_exc())


def server(port: int):
    setup_logging()
    
    mutex = GlobalMutex(GLOBAL_MUTEX_NAME)
    # We need to check again if the server is running since one could have been started since the
    # check performed in `main` (which is done before starting the daemon to avoid doing that if not required).
    already_running = not mutex.acquire()
    if already_running:
        log_info(f'server is already running on port {port}')
        return

    try:
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.settimeout(1.0)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(('0.0.0.0', port))
        server.listen(-1)
    except:
        log_error(traceback.format_exc())
        mutex.release()
        mutex.close()
        return

    while True:
        try:
            if server_loop(server):
                break
        except KeyboardInterrupt:
            log_error('Interrupted with CTRL+C, exiting...')
            break
        except MissingFslCmdsDirException:
            log_error('`fsl_cmds` directory was not found for the project you are recompiling - please recompile using the IDE to generate this directory')
            continue
        except:
            log_error(traceback.format_exc())
            continue

    mutex.release()
    mutex.close()


def unix_daemonize():
    try:
        pid = os.fork()
        if pid > 0:
            sys.exit(0)
    except OSError as e:
        print("fork #1 failed: %d (%s)" % (e.errno, e.strerror))
        sys.exit(1)

    # decouple from parent environment
    os.chdir('/')
    os.setsid()
    os.umask(0)

    # do second fork
    try:
        pid = os.fork()
        if pid > 0:
            sys.exit(0)
    except OSError as e:
        print("fork #2 failed: %d (%s)" % (e.errno, e.strerror))

    sys.stdout.flush()
    sys.stderr.flush()


def windows_daemon(port: int):
    flags = 0
    flags |= 0x00000008  # DETACHED_PROCESS
    flags |= 0x00000200  # CREATE_NEW_PROCESS_GROUP
    flags |= 0x08000000  # CREATE_NO_WINDOW
    subprocess.Popen([sys.executable, str(CURRENT_SCRIPT), '--port', str(port)], close_fds=True, creationflags=flags)


def main():
    parser = argparse.ArgumentParser()
    # In order to allow VS to call this script with an empty string for port, we need to make it a string argument and set to the default port if empty
    parser.add_argument('--port', default=DEFAULT_PORT, help=f'Port used for the socket server (default: {DEFAULT_PORT})')
    parser.add_argument('--daemon', action='store_true', help='Run the server as a daemon process instead of directly in the commandline')
    parser.add_argument('--kill', action='store_true', help='Kill the currently running server (if there is any)')
    args = parser.parse_args()
    if not args.port:
        args.port = DEFAULT_PORT
    else:
        args.port = int(args.port)

    if args.kill:
        if send_message(Msg.KILL_SERVER, args.port):
            print(f'{LOG_PREFIX}: killed server on port {args.port}')
        else:
            print(f'{LOG_PREFIX}: there are no servers running on port {args.port}')
        return

    mutex = GlobalMutex(GLOBAL_MUTEX_NAME)
    if mutex.acquire():
        already_running = False
        mutex.release()
    else:
        already_running = True
    mutex.close()

    if already_running:
        print(f'{LOG_PREFIX}: server is already running on port {args.port}')
        sys.exit(1)

    if not args.daemon:
        print(f'{LOG_PREFIX}: listening on port {args.port}...')
        enable_adb_port_reversal(args.port)
        server(args.port)
        return
    
    print(f'{LOG_PREFIX}: starting daemon process on port {args.port}...')
    if os.name == 'nt':
        windows_daemon(args.port)
    else:
        enable_adb_port_reversal(args.port)
        unix_daemonize()
        server(args.port)


if __name__ == '__main__':
    main()
