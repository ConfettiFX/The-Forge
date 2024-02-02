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

"""
Forge Material Compiler
"""

import os, sys, argparse
import string
from enum import Enum

class TextureFlags(Enum):
    NONE = 0
    SRGB = 1

class Texture:
    def __init__(self, name, flags):
        self.name = name
        self.flags = flags
        self.unique_name = name + "-".join([ f.name for f in self.flags ])

        self.combined_flags = 0
        for f in flags: self.combined_flags |= f.value

class TextureSet:
    def __init__(self):
        self.name = ""
        self.textures = []

class ShaderSet:
    def __init__(self):
        self.name = ""

        self.shaders = [ "", "", "", "", "", "" ]
        
        self.bindings = []

    @staticmethod
    def shader_index(token : str):
        types = [ "v", "f", "h", "d", "g", "c" ]
        return types.index(token)
    
    @staticmethod
    def shader_type(index):
        types = [ "v", "f", "h", "d", "g", "c" ]
        return types[index]
        
    @staticmethod
    def shader_extension(index):
        types = [ ".vert", ".frag", ".tesc", "tese", ".geom", ".comp" ]
        return types[index]

class MaterialSet:    
    def __init__(self):
        self.shader_set = ""
        self.shader_set_idx = 0
        self.texture_set = ""

class Material:    
    def __init__(self):
        self.filename = ""

        self.texture_sets = []
        self.shader_sets = []
        self.material_sets = []

class MaterialParser:
    out = Material()

    # Active set while parsing (only one can be valid)
    texture_set = None
    shader_set = None
    material_set = None

    def end_set(self):
        if self.texture_set:
            self.out.texture_sets += [ self.texture_set ]
            assert(not self.shader_set)
            assert(not self.material_set)
        if self.shader_set:
            self.out.shader_sets += [ self.shader_set ]
            assert(not self.texture_set)
            assert(not self.material_set)
        if self.material_set:
            self.out.material_sets += [ self.material_set ]
            assert(not self.shader_set)
            assert(not self.texture_set)

        self.texture_set = None
        self.shader_set = None
        self.material_set = None

    def unknown_token(self, line_number, tokens):
        set_name = ""
        set_message = ""
        if self.texture_set:
            set_message = " while parsing TextureSet '{0}'"
            set_name = self.texture_set.name;
        elif self.shader_set:
            set_message = " while parsing ShaderSet '{0}'"
            set_name = self.shader_set.name;
        elif self.material_set:
            set_message = " while parsing MaterialSet '{0}'"
            set_name = self.material_set.name;

        filename = self.out.filename
        message = "FMC: {0}:{1}: Warning: Unknown token '{2}'".format(filename, line_number, tokens[0]) + set_message.format(set_name)
        print(message)
    
    def parse(self, filename : string, lines : list):
        self.out = Material()
        self.out.filename = filename

        for index, line in enumerate(lines):
            line.strip()
            if len(line) == 0 or line.startswith('#'): continue

            tokens = line.split(' ')
            line_number = index + 1

            if tokens[0] == 'S':
                self.end_set()
                self.shader_set = ShaderSet()
                self.shader_set.name = tokens[1]
            elif tokens[0] == 'T':
                self.end_set()
                self.texture_set = TextureSet()
                self.texture_set.name = tokens[1]
            elif tokens[0] == 'M':
                self.end_set()
                self.material_set = MaterialSet()
                self.material_set.name = tokens[1]
            elif self.shader_set:
                if tokens[0] == 's':
                    assert(self.shader_set)
                    shader_idx = self.shader_set.shader_index(tokens[1])
                    self.shader_set.shaders[shader_idx] = tokens[2]
                elif tokens[0] == 'b':
                    self.shader_set.bindings += [ tokens[1] ]
                else:
                    self.unknown_token(line_number, tokens)
            elif self.texture_set:
                if tokens[0] == 't':
                    flags = []
                    if len(tokens) > 2:
                        if tokens[2].lower() == "srgb":
                            flags += [ TextureFlags.SRGB ]
                    
                    self.texture_set.textures += [ Texture(tokens[1], flags) ]

                else:
                    self.unknown_token(line_number, tokens)
            elif self.material_set:
                if tokens[0] == 's': self.material_set.shader_set = tokens[1]
                elif tokens[0] == 't': self.material_set.texture_set = tokens[1]
                else:
                    self.unknown_token(line_number, tokens)
            else:
                self.unknown_token(line_number, tokens)
        
        self.end_set()
        return self.out

def build_material_header(mat):
    total_num_shaders = 0
    for shader_set in mat.shader_sets:
        total_num_shaders +=  sum([ 1 if shader else 0 for shader in shader_set.shaders ])

    # TODO: Currently if two sets use the same texture we still allocate memory for 2 and then have duplicate names in runtime, 
    #       we could remove that by only counting unique textures (C++ would also need to account for that).
    #       On the other hand, the texture is only uploaded once to the GPU because we reuse the global table, so this is not a big issue
    #       (same happens for shaders above, but that's a smaller problem as we usually have much less shaders than textures)
    total_num_textures = sum([ len(texture_set.textures) for texture_set in mat.texture_sets ])

    max_shader_bindings = max([ len(shader_set.bindings) for shader_set in mat.shader_sets ])
    max_textures_in_set = max([ len(texture_set.textures) for texture_set in mat.texture_sets ])

    header = "{0} {1} {2} {3} {4} {5} {6}".format(len(mat.shader_sets), len(mat.texture_sets), len(mat.material_sets), total_num_shaders, total_num_textures, max_shader_bindings, max_textures_in_set)
    return ":FMC " + header

def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--directory', help='input directory', required=True)
    parser.add_argument('-o', '--output', help='output directory', required=True)
    parser.add_argument('--verbose', default=False, action='store_true')
    # TODO: Add incremental compilation
    #parser.add_argument('--incremental', default=False, action='store_true')
    args = parser.parse_args()
    return args

def make_full_filepath(dir, filename):
    return os.path.normpath(os.path.abspath(dir + '/' + filename)).replace(os.sep, '/')

def main():
    args = get_args()

    in_directory = args.directory
    out_directory = args.output
    verbose = args.verbose

    all_mat_filenames = []
    for f in os.listdir(in_directory): 
        if f.endswith('.fmat'): all_mat_filenames += [ f ]
    
    all_materials = []

    # 1. Parse all material files
    for filename in all_mat_filenames:
        lines = open(make_full_filepath(in_directory, filename)).read().split("\n")

        parser = MaterialParser()
        new_material = parser.parse(filename, lines)

        # TODO: Run some validation and optimization on new_material:
        #         - check for duplicate ShaderSet, TextureSet and MaterialSet names.
        #         - remove ShaderSets, TextureSets that are not used (not referenced by MaterialSets)

        all_materials += [ new_material ]

    unique_shader_set_identifiers = []
    unique_shader_sets = []
    unique_textures = {}
    total_texture_refs = 0
    total_shader_refs = 0

    # 2. Collect all unique resources
    for mat in all_materials:
        for shader_set in mat.shader_sets:
            total_shader_refs += 1

            # TODO: we could hash this string so that we don't compare strings later on
            shader_names = [ shader if shader else "<None>" for shader in shader_set.shaders ]
            unique_identifier = ",".join(shader_names)
            if unique_shader_set_identifiers.count(unique_identifier) == 0:
                unique_shader_set_identifiers += [ unique_identifier ]
                unique_shader_sets += [ shader_set.name ]

        for texture_set in mat.texture_sets:
            for texture in texture_set.textures:
                total_texture_refs += 1
                if texture.unique_name not in unique_textures:
                    unique_textures[texture.unique_name] = texture

        # Set indexes for shader/texture_sets
        for material_set in mat.material_sets:
            for index, shader_set in enumerate(mat.shader_sets):
                if shader_set.name == material_set.shader_set:
                    material_set.shader_set_idx = index
                    break
            
            for index, texture_set in enumerate(mat.texture_sets):
                if texture_set.name == material_set.texture_set:
                    material_set.texture_set_idx = index
                    break

    # Start outputting data
    os.makedirs(out_directory, exist_ok=True)

    unique_texture_idxs = list(unique_textures.keys())

    # 3. Output compiled materials that reference resources using unique ids (indexes in the unique resource arrays from previous step)
    for mat in all_materials:
        lines = [ build_material_header(mat) ]

        # shader sets
        for shader_set in mat.shader_sets:
            lines += [ "S {0}".format(unique_shader_sets.index(shader_set.name)) ]

            for index, name in enumerate(shader_set.shaders):
                if name: lines += [ "s " + shader_set.shader_type(index) + " " + name + shader_set.shader_extension(index) ]
            
            lines += [ "b " + " ".join(shader_set.bindings) ]
        
        # texture sets
        for texture_set in mat.texture_sets:
            lines += [ "T" ]
            for texture in texture_set.textures:
                assert(texture.unique_name in unique_textures)
                # TODO: Here we are outputing the texture name (that we later parse in runtime) but we could avoid it if the material library loads a table with all texture names at startup
                #       This might be a problem if we have A LOT of textures, but worth considering
                new_line = "t {0} {1} {2}".format(texture.name, texture.combined_flags, unique_texture_idxs.index(texture.unique_name))
                lines += [ new_line ]

        # material sets
        for material_set in mat.material_sets:
            lines += [ "M " + material_set.name ]
            lines += [ "s {0}".format(material_set.shader_set_idx) ]
            lines += [ "t {0}".format(material_set.texture_set_idx) ]

        lines += [ "" ]
        output_filepath = make_full_filepath(out_directory, mat.filename)
        with open(output_filepath, "w", newline='') as out_file:
            out_file.write("\n".join(lines))

    # 4. Output the data we need to initialize MaterialLibrary
    material_lib_ini_file = make_full_filepath(out_directory, "material_lib.ini")
    with open(material_lib_ini_file, "w", newline='') as out_file:
        lines = [ "{0} {1}".format(len(unique_shader_sets), len(unique_textures)) ]
        out_file.write("\n".join(lines))

    # 5. Output material manifest for debugging purposes, this manifest maps texture and shader_set global ids to their names
    manifest_filepath = make_full_filepath(out_directory, "compiledMaterialsManifest.txt")
    with open(manifest_filepath, "w", newline='') as manifest:
        manifest.write("Textures:\n")
        for key, texture in unique_textures.items():
            manifest.write("{0} {1} {2}\n".format(unique_texture_idxs.index(texture.unique_name), texture.name, texture.combined_flags))

        manifest.write("ShaderSets (vert, frag, hull, domain, geom, comp):\n")
        for index, shader_set_name in enumerate(unique_shader_sets):
            manifest.write("{0} {1} {2}\n".format(index, shader_set_name, unique_shader_set_identifiers[index]))

    if verbose:
        print("Forge Material Compiler:")
        print("  Material Files: {0}".format(len(all_mat_filenames)))
        print("  Shaders:  Total={0}   Unique={1}".format(total_shader_refs, len(unique_shader_set_identifiers)))
        print("  Textures: Total={0}   Unique={1}".format(total_texture_refs, len(unique_textures)))

    return 0

if __name__ == '__main__':
    sys.exit(main())
