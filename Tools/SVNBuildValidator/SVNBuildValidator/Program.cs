using System;
using System.IO;
using System.Diagnostics;

namespace SVNBuildValidator
{
	class Program
	{
		static void Main(string[] args)
		{
			string curDir = Directory.GetCurrentDirectory();

			string dir = Directory.GetParent(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location)).FullName;
			Directory.SetCurrentDirectory(dir);

			string pythonExecutable = "";
			string buildScript = "PyBuild.py";
			var paths = Environment.GetEnvironmentVariable("PATH").Split(';');

			foreach (var v in paths)
			{
				if (v.EndsWith("Python27") || v.EndsWith("Python27\\") || v.EndsWith("Python27/"))
				{
					pythonExecutable = Path.Combine(v, "python.exe");
					break;
				}
			}

			if (pythonExecutable == "")
			{
				Console.Error.WriteLine("Python 2.7 Executable Not found on path. Please install python 2.7 or add the Python directory to your path");
				Environment.Exit(1);
			}

			ProcessStartInfo info = new ProcessStartInfo
			{
				UseShellExecute = true,
				FileName = pythonExecutable,
				Arguments = buildScript,
			};

			Process buildProc = Process.Start(info);
			buildProc.WaitForExit();
			Environment.Exit(buildProc.ExitCode);
		}

		static bool ExistsOnPath(string fileName)
		{
			return GetFullPath(fileName) != null;
		}

		static string GetFullPath(string fileName)
		{
			if (File.Exists(fileName))
				return Path.GetFullPath(fileName);

			var values = Environment.GetEnvironmentVariable("PATH");
			foreach (var path in values.Split(';'))
			{
				var fullPath = Path.Combine(path, fileName);
				if (File.Exists(fullPath))
					return fullPath;
			}
			return null;
		}
	}
}