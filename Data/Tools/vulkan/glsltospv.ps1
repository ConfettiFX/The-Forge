#

# glsltospv.ps1

#
param([String]$directory, [Int32]$useconfigfile) #Must be the first statement in your script

if( [string]::IsNullOrEmpty($directory) ) {
Write-Host "Description: Compiles the glsl shaders in given directory and stores generated spir-v code in the Binary folder"
Write-Host "Usage:"
Write-Host ""
Write-Host "    powershell.exe -command ""&'.\glsltospv.ps1' [-directory] '<path_to_glsl_shader_directory>'"
write-host "                   [-useconfigfile]"
Write-Host ""       
Write-Host "       directory = Directory with the glsl shaders"       
Write-Host "   useconfigfile = Optional switch to use the config file present in the directory"       
write-host ""

exit
}

# create this function in the calling script
function Get-ScriptDirectory { Split-Path $MyInvocation.ScriptName }

$oldDir = $(Get-ScriptDirectory)
# generate the path to the script in the utils directory:
$relativeDir = Join-Path (Get-ScriptDirectory) $directory

$binaryDir = Join-Path $relativeDir "//Binary"

New-Item -Path $binaryDir -ItemType Directory -Force

cd $relativeDir
$files = Get-ChildItem -Path ".\*" -Include *.frag, *.vert, *.comp, *.tesc, *.tese

if ($useconfigfile -ne 0) {
	foreach($file in $files) {
		$outFile = "Binary\" + $file.Name + ".spv"
		glslangValidator.exe -V config.conf $file.Name -o $outFile
	}
}
else {
	foreach($file in $files) {
		$outFile = "Binary\" + $file.Name + ".spv"
		glslangValidator.exe -V $file.Name -o $outFile
	}
}

cd $oldDir