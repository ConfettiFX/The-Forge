#!/usr/bin/python
#----------------------------------------------------------------------------#
#                                                                            #
# ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  #
# and distributed under the MIT License (MIT).                               #
#                                                                            #
# Copyright (c) 2017 Guillaume Blanc                                         #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, sublicense,   #
# and/or sell copies of the Software, and to permit persons to whom the      #
# Software is furnished to do so, subject to the following conditions:       #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
#----------------------------------------------------------------------------#

import os, glob
import sys
import itertools
import string
import re
import time

def recurse_files(_folder, _filter):
    # Iterate files.
    for i in glob.iglob(os.path.join(_folder, _filter)):
        yield i
    # Iterate folders...
    for i in glob.iglob(os.path.join(_folder, '*/')):
        #... and recurse them.
        for j in recurse_files(i, _filter):
            if j.find('\\extern\\') == -1 and j.find('/extern/') == -1 and j.find('\\build\\') == -1 and j.find('/build/') == -1 and j.find('\\src_fused\\') == -1 and j.find('/src_fused/') == -1:
                yield j

license_text = "\
//----------------------------------------------------------------------------//\n\
//                                                                            //\n\
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //\n\
// and distributed under the MIT License (MIT).                               //\n\
//                                                                            //\n\
// Copyright (c) 2017 Guillaume Blanc                                         //\n\
//                                                                            //\n\
// Permission is hereby granted, free of charge, to any person obtaining a    //\n\
// copy of this software and associated documentation files (the \"Software\"), //\n\
// to deal in the Software without restriction, including without limitation  //\n\
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //\n\
// and/or sell copies of the Software, and to permit persons to whom the      //\n\
// Software is furnished to do so, subject to the following conditions:       //\n\
//                                                                            //\n\
// The above copyright notice and this permission notice shall be included in //\n\
// all copies or substantial portions of the Software.                        //\n\
//                                                                            //\n\
// THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //\n\
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //\n\
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //\n\
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //\n\
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //\n\
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //\n\
// DEALINGS IN THE SOFTWARE.                                                  //\n\
//                                                                            //\n\
//----------------------------------------------------------------------------//\n\
\n\
#"

def process_file(_file, _is_h):
    # Read
    fr = open(_file, 'rt')
    if fr == None:
        print "Failed to read file " + _file
        return False
    
    text = fr.read();
    fr.close()

    # Prepares output
    output = ""
    modified = False;

    # Test for a valid license
    find_license = string.find(text, license_text)
    if find_license == -1:
        # Replaces license up to the first '#'
        first_define = string.find(text, '#')
        if first_define != -1:
            output = license_text + text[first_define + 1:]
        else:
            output = license_text + text
        modified = True
    else:
        output = text
        
    # '#' must be found as it is part of the license
    first_define = string.find(output, '#')

    if _is_h:
        # Test for a valid #define GUARD

        # Removes pragma once
        pragma_once = '#pragma once'
        pragma_found = string.find(output[first_define:], pragma_once)
        if  pragma_found == 0:
            output = output[:first_define] + output[first_define + len(pragma_once):]
            modified = True

        # Prepares #define GUARD
        guard = string.upper(_file)
        to_replace = ['/', '\\', '.', '-']
        for i in to_replace:
            guard = string.replace(guard, i, '_') 
        to_remove = ['INCLUDE_', 'SRC_']
        for i in to_remove:
            guard = string.replace(guard, i, '') 
        guard = 'OZZ_' + guard + '_'
        guard = re.sub('_+', '_', guard)

        header = '#ifndef ' + guard + '\n' + '#define ' + guard + '\n'
        footer = '#endif  // ' + guard + '\n'

        # Test for a valid header/footer
        found_match = re.search('^#ifndef (?P<found_guard>.+)\n^#define (?P=found_guard)\n(.*\n)*^#endif  // (?P=found_guard)',
                                output[first_define:],
                                re.MULTILINE);
        if found_match == None:
            output = output[:first_define] + header + output[first_define:]
            if not output.endswith('\n'):
                output += '\n'
            output += footer;
            modified = True
        elif found_match.group('found_guard') != guard:
            output = string.replace(output, found_match.group('found_guard'), guard)
            modified = True

        # Needs at least 1 \n
        if not output.endswith('\n'):
            output += '\n'

        # remove \n after guard begin   
        guard_pos = output.find(header) + len(header) - 1
        if not output.startswith('\n\n', guard_pos):
            output = output[:guard_pos] + '\n' + output[guard_pos:]
            modified = True
        while output.startswith('\n\n\n', guard_pos):
            output = output[:guard_pos] + output[guard_pos+1:]
            modified = True

        # remove \n before guard end
        guard_pos = output.find(footer)
        while output.endswith('\n\n', 0, guard_pos):
            output = output[:guard_pos-1] + output[guard_pos:]
            guard_pos = output.find(footer)
            modified = True

    # must end with a single \n
    if not output.endswith('\n'):
        output += '\n'
        modified = True
    else:
        while output.endswith('\n\n'):
            output = output[:len(output) - 1]
            modified = True

    # Write
    if modified:
        fw = open(_file, 'wt')
        if fw == None:
            print "Failed to write file " + _file
            modified = False
        fw.write(output)
        fw.close()
        print _file + ' modified'

    return modified

def process_h(_file):
    return process_file(_file, True)

def process_cc(_file):
    return process_file(_file, False)
    
def main():
    # Process .h
    for i in recurse_files('../../', '*.h'):
        process_h(i)
    # Process .cc
    for i in recurse_files('../../', '*.cc'):
        process_cc(i)
    #
    print 'Terminated'

    while 1:
        time.sleep(10)
        
main()
