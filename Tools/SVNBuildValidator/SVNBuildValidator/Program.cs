/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

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