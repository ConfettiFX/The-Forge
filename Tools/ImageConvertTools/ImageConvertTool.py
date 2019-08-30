"""
Helper script for using Basisu.

Usage:
$ basisu.py fixall
  - TODO:
"""

import sys
import os
from shutil import copyfile
import subprocess
import threading
import time
import platform


# LIST OF COMMANDS
# -----------------------------
CMD_FIXALL = "fixall"
# -----------------------------

# HELPERS ----------------------------------------------------------------------------
def PrintHelp():
    print(' ')
    print('GLTF texture folder Helper for managing texture file names and folder structure.')
    print(' ')
    print('Usage:')
    print('    basisu.py [command] [args]')
    print(' ')
    print('Available Commands:')  
    print('    ', CMD_FIXALL)
    print(' ')

def CheckCommand(cmd):
    if ( not cmd == CMD_FIXALL
    ):
        return False
    return True



# COMMANDS ---------------------------------------------------------------------------
def ExecCmdInit():
    if len(sys.argv) < 3:
        print('init: missing argument: please provide the folder name to initialize.')
        return

    folder_name = sys.argv[2]
    
    # create the folder provided in the argument
    try:
        os.mkdir(folder_name)
    except OSError:
        print('Could not create directory: ', folder_name)

    # change dir and create subfolders
    os.chdir(folder_name)
    try:
        os.mkdir("2K")
    except OSError:
        print('Could not create directory: 2K')
    try:
        os.mkdir("1K")
    except OSError:
        print('Could not create directory: 1K')

    return

def ExecCmdCheck():
    print('ExecCmdCheck')
    return

def FindInArray(find_array, find):
	for f in find_array:
		if f.lower() in find.lower():
			return 1
	return 0
	
def TexConvertFunction(basisu, filename, options):
	dir_path = os.path.dirname(os.path.realpath(__file__))
	basisu_fullpath = dir_path + "\\" + basisu
	command = '"%s" %s %s'%(
        basisu_fullpath,
		options,
        filename
    )
	
	print("Converting [" + filename + "]")
	DEVNULL = open(os.devnull, 'wb')
	p = subprocess.call(command, stdout=DEVNULL, stderr=DEVNULL)
	return

def KTXCompressFunction(compress, output, filename, options):
	#output = os.path.splitext(filename)[0] + ".ktx"
	command = '"%s" -o "%s" %s "%s"'%(
        compress,
        output,
        options,
		filename,
    )
	print(command)
	DEVNULL = open(os.devnull, 'wb')
	p = subprocess.call(command, stdout=DEVNULL, stderr=DEVNULL, shell=True)
	# p = subprocess.call(command)
	return

def ExecCmdFixAll():
	print('ExecCmdFixAll')
	
	start = time.time()
	
	basisu = "basisu.exe"
	ktxcompress = "ImageConvertTools/img2ktx.exe"
	
	relative_input_dir = ""
	relative_output_dir = ""
	# expect user to provide which folder to fix
	if len(sys.argv) < 4:
		print('fixname: missing argument: using directory of script.')
	else:
		relative_input_dir = sys.argv[2]
		relative_output_dir = sys.argv[3]

	rootdir = os.path.join(os.path.dirname(os.path.realpath(__file__)), relative_input_dir)
	print("Input directory: " + rootdir)
	outputdir = os.path.join(os.path.dirname(os.path.realpath(__file__)), relative_output_dir)
	print("Output directory: " + outputdir)

	max_open_files = 128

	extensions = [ ".png" ]
	tasks = []
	files_to_fix = []
	temp_files = []
	basis_files = []
	files_open = 0	
	bc4_list = [ ]	

	print("Convert all PNG to BASIS")
	
	for subdir, dirs, files in os.walk(rootdir):
		for file in files:
			ext = os.path.splitext(file)[-1].lower()
			if ext in extensions:
				filename = (os.path.join(subdir, file))
				files_to_fix.append(filename)
	
	for file in files_to_fix:
		options = "-linear -level 4 "
		if "normal" in file.lower():			
			options = options + "-userdata0 1 " + "-seperate_rg_to_color_alpha "
		ext = os.path.splitext(file)[-1].lower()
		temp_files.append(os.path.splitext(file)[0] + ".png")	
		
		basis_fn = os.path.basename(file)
		basis_files.append(os.path.splitext(basis_fn)[0] + ".basis")

		thread_args = (basisu, file, options)
		t = threading.Thread(target=TexConvertFunction, args=thread_args)
		t.start()
		tasks.append(t)
		files_open = files_open + 1
		if files_open > max_open_files:
			for thread in tasks:
				thread.join()
			files_open = 0
			tasks = []
		
	for thread in tasks:
		thread.join()

	print("Copy all BASIS fiiles to Output directory")

	for file in basis_files:
		currentFile = os.path.basename(file)
		filename = (os.path.join(os.getcwd(), file))	
		print("Copying " + currentFile + " to output dir")
		copyfile(filename, os.path.join(outputdir, file))

	for file in basis_files:
		os.remove(os.path.abspath(file))

	for thread in tasks:
		thread.join()
		
	print("Convert all textures to mobile compressed format")
		
	extensions = [ ".png" ]
	tasks = []
	ktx_files = []
	files_to_fix = []
	files_open = 0
	
	for subdir, dirs, files in os.walk(rootdir):
		for file in files:
			ext = os.path.splitext(file)[-1].lower()
			if ext in extensions:
				filename = (os.path.join(subdir, file))
				files_to_fix.append(filename)

	for file in files_to_fix:
		options = "-m"
		ext = os.path.splitext(file)[-1].lower()
		compress = ktxcompress		
		quality = "fast"

		ktx_fn = os.path.basename(file)
		new_name = os.path.splitext(ktx_fn)[0] + ".ktx"
		ktx_files.append(new_name)

		astc = "ASTC8x8"
		
		if "normal" in file.lower():
			options = options + " -f ASTC4x4 -flags \"-normal_psnr -%s\""%(quality)		
		else:
			options = options + " -f %s -flags \"-%s\""%(astc, quality)			
		
		# KTXCompressFunction(compress=compress, filename=file, options=options)
		thread_args = (compress, outputdir + "\\" + new_name, file, options)
		t = threading.Thread(target=KTXCompressFunction, args=thread_args)
		t.start()
		tasks.append(t)
		files_open = files_open + 1
		if files_open > max_open_files:
			for thread in tasks:
				thread.join()
			files_open = 0
			tasks = []
			
	for thread in tasks:
		thread.join()
	
	
		
	end = time.time()
	print(end - start)

	return


# ENTRY POINT --------------------------------------------------------------------------
def Main():
    # arg check
    if len(sys.argv) < 2:
        PrintHelp()
        return

    # command check
    cmd = sys.argv[1]
    if not CheckCommand(cmd):
        print('Incorrect command: ', cmd)
        PrintHelp()
        return
    
    # exec commands
    if cmd == CMD_FIXALL:
        ExecCmdFixAll()
    


# print '# args: ', len(sys.argv)
# print ':: ', str(sys.argv)
if __name__ == "__main__":
    Main()

