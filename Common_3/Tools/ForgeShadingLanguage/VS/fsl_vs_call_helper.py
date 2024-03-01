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

import os, sys


if __name__ == '__main__':

    parent_dir = os.path.sep.join(os.path.abspath(__file__).split(os.path.sep)[:-2])
    sys.path.append(parent_dir)
    from fsl import main

    assert len(sys.argv) == 13
    script, windowsSDK, dst, binary_dst, src, lang, compile, verbose, config, platform, tmp_path, reload_server_port, cache_args = sys.argv
    tmp_path = tmp_path.strip()
    src = os.path.abspath(src)
    dst = os.path.abspath(dst)
    binary_dst = os.path.abspath(binary_dst)


    windowsSDKPaths = windowsSDK.split(';')
    if windowsSDKPaths:
        os.environ['FSL_COMPILER_FXC'] = windowsSDKPaths[0]

    # if no platform specified, use these defaults
    if not lang and 'ANDROID' in platform.upper():
        lang = 'ANDROID'
    elif not lang and 'NX64' in platform.upper():
        lang = 'SWITCH'
    elif not lang and 'ORBIS' in platform.upper():
        lang = 'ORBIS'
    elif not lang and 'PROSPERO' in platform.upper():
        lang = 'PROSPERO'
    elif not lang and 'SCARLETT' in platform.upper():
        lang = 'SCARLETT'
    elif not lang and 'XBOX' in platform.upper():
        lang = 'XBOX'

    reload_server_args = []
    if reload_server_port == '':
        reload_server_args = ['--reloadServerPort', '6543']
    else:
        try:
            reload_server_args = ['--reloadServerPort', str(int(reload_server_port))]
        except ValueError:
            reload_server_args = []
    
    if not (cache_args.lower() == 'no' or cache_args.lower() == 'false' or cache_args.lower() == '0'):
        reload_server_args.append('--cache-args')

    sys.argv = [
        script,
        src,
        '-d', dst,
        '-b', os.path.abspath(binary_dst),
        '-i', os.path.abspath(tmp_path),
        '-l', lang,
        *reload_server_args,
        '--incremental',
    ]
    if 'debug' in config.lower():
        sys.argv += ['--debug']
    if verbose=='true': sys.argv += ['--verbose']
    if compile=='true': sys.argv += ['--compile']

    sys.exit(main())