using System;
using System.Collections.Generic;
using System.ComponentModel.Design;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio;
using System.IO;
using EnvDTE;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Linq;

namespace ShaderBuildCommand
{
    /// <summary>
    /// Command handler
    /// </summary>
    internal sealed class ShaderBuildCommand
    {
        /// <summary>
        /// Command ID.
        /// </summary>
        public const int CommandId = 0x0100;

        /// <summary>
        /// Command menu group (command set GUID).
        /// </summary>
        public static readonly Guid CommandSet = new Guid("9c398598-0747-4543-ae06-f418fa910891");

        /// <summary>
        /// VS Package that provides this command, not null.
        /// </summary>
        private readonly Package package;

        /// <summary>
        /// Initializes a new instance of the <see cref="ShaderBuildCommand"/> class.
        /// Adds our command handlers for menu (commands must exist in the command table file)
        /// </summary>
        /// <param name="package">Owner package, not null.</param>
        private ShaderBuildCommand(Package package)
        {
            if (package == null)
            {
                throw new ArgumentNullException("package");
            }

            this.package = package;

            OleMenuCommandService commandService = this.ServiceProvider.GetService(typeof(IMenuCommandService)) as OleMenuCommandService;
            if (commandService != null)
            {
                var menuCommandID = new CommandID(CommandSet, CommandId);

                // AND REPLACE IT WITH A DIFFERENT TYPE
                var menuItem = new OleMenuCommand(this.MenuItemCallback, menuCommandID);
                commandService.AddCommand(menuItem);
            }
        }

        /// <summary>
        /// Gets the instance of the command.
        /// </summary>
        public static ShaderBuildCommand Instance
        {
            get;
            private set;
        }

        /// <summary>
        /// Gets the service provider from the owner package.
        /// </summary>
        private IServiceProvider ServiceProvider
        {
            get
            {
                return this.package;
            }
        }

        /// <summary>
        /// Initializes the singleton instance of the command.
        /// </summary>
        /// <param name="package">Owner package, not null.</param>
        public static void Initialize(Package package)
        {
            Instance = new ShaderBuildCommand(package);
        }

        struct ShaderCompileData
        {
            public String fileName;
            public String outFileName;
            public String macros;
            public ShaderLanguage language;
            public System.Diagnostics.Process process;
        }

        struct ShaderMacro
        {
            public String definition;
            public String value;
        }

        /// <summary>
        /// This function is the callback used to execute the command when the menu item is clicked.
        /// See the constructor to see how the menu item is associated with this function using
        /// OleMenuCommandService service and MenuCommand class.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event args.</param>
        private void MenuItemCallback(object sender, EventArgs e)
        {
            var myCommand = sender as OleMenuCommand;

            EnvDTE80.DTE2 applicationObject = this.ServiceProvider.GetService(typeof(DTE)) as EnvDTE80.DTE2;
            object[] selectedItems = (object[])applicationObject.ToolWindows.SolutionExplorer.SelectedItems;
            var processArray = new List<ShaderCompileData>();

            FindHLSLCompiler(out string fxc, "fxc.exe");
            FindHLSLCompiler(out string dxc, "dxc.exe");

            foreach (EnvDTE.UIHierarchyItem selectedUIHierarchyItem in selectedItems)
            {
                if (selectedUIHierarchyItem.Object is EnvDTE.ProjectItem)
                {
                    EnvDTE.ProjectItem item = selectedUIHierarchyItem.Object as EnvDTE.ProjectItem;
                    int size = item.FileCount;
                    string fileName = item.FileNames[0];
                    if (IsShaderFile(fileName, out ShaderType shaderType))
                    {
                        StreamReader reader = new StreamReader(fileName);
                        if (reader != null)
                        {
                            string content = reader.ReadToEnd();
                            DetermineShaderLanguage(content, out ShaderLanguage language);
                            ProcessStartInfo info = null;
                            FindShaderMacros(content, language, out string[] macros);
                            String outFile = Path.GetFileName(fileName) + ".bin";
                            String inFileFolder = Path.GetDirectoryName(fileName);
                            String outFileFullPath = String.Format("{0}\\{1}", inFileFolder, outFile);

                            switch (language)
                            {
                                case ShaderLanguage.HLSL:
                                    {
                                        DetermineShaderTarget(shaderType, fxc, dxc, out string shaderTarget, out string compiler);
                                        if (compiler == "")
                                        {
                                            WriteToOutputWindow("Cannot find HLSL Compiler. Make sure your Windows SDK has fxc.exe (dxc.exe for raytracing shader)");
                                            continue;
                                        }
                                        GenerateHLSLArguments(shaderType, fileName, outFile, shaderTarget, out string compilerArguments);
                                        info = new ProcessStartInfo
                                        {
                                            UseShellExecute = false,
                                            FileName = String.Format("\"{0}\"", compiler),
                                            Arguments = compilerArguments,
                                            RedirectStandardError = true,
                                            RedirectStandardOutput = true,
                                            CreateNoWindow = true,
                                        };
                                        break;
                                    }
                                case ShaderLanguage.VULKAN_GLSL:
                                    {
                                        String configFileName = "config.conf";
                                        String vulkanSDK = Environment.GetEnvironmentVariable("VULKAN_SDK");
                                        String glslangValidator = vulkanSDK + "\\Bin\\glslangValidator.exe";
                                        bool useConfigFile = File.Exists(fileName + "\\..\\" + configFileName);
                                        configFileName = String.Format("\"{0}\"", Path.Combine(fileName + "\\..\\", configFileName));
                                        info = new ProcessStartInfo
                                        {
                                            UseShellExecute = false,
                                            FileName = glslangValidator,
                                            Arguments = String.Format("{0} -V \"{1}\" -o \"{2}\"", useConfigFile ? configFileName : " ", fileName, outFileFullPath),
                                            RedirectStandardError = true,
                                            RedirectStandardOutput = true,
                                            CreateNoWindow = true,
                                        };
                                        break;
                                    }
                            }

                            for (int i = 0; i < macros.Length; ++i)
                            {
                                String prevArgs = info.Arguments;
                                String macroFormat = String.Format("_{0}_{1}", processArray.Count, outFile);

                                info.Arguments += macros[i];
                                info.Arguments = info.Arguments.Replace(outFile, macroFormat);
                                WriteToOutputWindow(String.Format("Compiling {0}:\n\t {1} {2}\n", fileName, info.FileName, info.Arguments));
                                processArray.Add(new ShaderCompileData
                                {
                                    fileName = fileName,
                                    outFileName = outFileFullPath.Replace(outFile, macroFormat),
                                    language = language,
                                    macros = macros[i].TrimStart().TrimEnd().Replace("-D", "").Replace("\"", ""),
                                    process = System.Diagnostics.Process.Start(info),
                                });
                                info.Arguments = prevArgs;
                            }
                        }
                    }
                    else
                    {
                        WriteToOutputWindow(String.Format("Not supported : {0} Supported extensions are {1}\n",
                            Path.GetFileName(fileName), String.Join(", ", shaderExtensions)));
                    }
                }
            }

            for (int i = 0; i < processArray.Count; ++i)
            {
                // glslangValidator error message format
                /*
				 * ERROR: C:\Users\agent47\Documents\Experimental\VisualStudio\ConsoleApp1\ConsoleApp1\skybox.frag:24: '' :  syntax error, unexpected IDENTIFIER
				 */
                String errorLog = processArray[i].language == ShaderLanguage.VULKAN_GLSL ?
                    processArray[i].process.StandardOutput.ReadToEnd() : processArray[i].process.StandardError.ReadToEnd();
                processArray[i].process.WaitForExit();
                if (processArray[i].process.ExitCode != 0)
                {
                    // For Vulkan format the error message so user can double click on it in output window to get to exact line with error
                    if (processArray[i].language == ShaderLanguage.VULKAN_GLSL)
                    {
                        errorLog = errorLog.Replace("ERROR: ", "");
                        String pattern = "([A-Za-z.0-9]+):(\\d+):";
                        Regex regex = new Regex(pattern);
                        String replacement = String.Format("$1($2):");
                        String formattedError = regex.Replace(errorLog, replacement);
                        errorLog = formattedError;
                    }
                    WriteToOutputWindow(String.Format("Failure : Shader {0} {1}\n{2}", Path.GetFileName(processArray[i].fileName), processArray[i].macros, errorLog));
                }
                else
                {
                    WriteToOutputWindow(String.Format("Success : Shader {0} {1} compiled successfully\n", Path.GetFileName(processArray[i].fileName), processArray[i].macros));
                }

                // Cleanup
                if (processArray[i].language == ShaderLanguage.VULKAN_GLSL)
                {
                    File.Delete(String.Format("SPIRV_OUTPUT_{0}.bin", i));
                }
                File.Delete(processArray[i].outFileName);
            }
        }

        enum ShaderLanguage { HLSL, VULKAN_GLSL };
        enum ShaderType { VERT, TESC, TESE, GEOM, FRAG, COMP,
            RGEN, RCHIT, RMISS, RINT, RAHIT, RCALL };
        string[] shaderExtensions = {   "vert", "tesc", "tese", "geom", "frag", "comp",
                                        "rgen", "rchit", "rmiss", "rint", "rahit", "rcall" };

        bool IsShaderFile(String fileName, out ShaderType shaderType)
        {
            for (int i = 0; i < shaderExtensions.Length; ++i)
            {
                if (fileName.EndsWith(shaderExtensions[i]))
                {
                    shaderType = (ShaderType)i;
                    return true;
                }
            }

            shaderType = (ShaderType)(-1);
            return false;
        }

        void DetermineShaderLanguage(String content, out ShaderLanguage language)
        {
            String vulkanPattern = "#version \\d+( core)?";
            Regex regex = new Regex(vulkanPattern);
            if (regex.IsMatch(content))
            {
                language = ShaderLanguage.VULKAN_GLSL;
            }
            else
            {
                language = ShaderLanguage.HLSL;
            }
        }

        void GenerateHLSLArguments(ShaderType shaderType, string fileName, string outFile, string shaderTarget, out string compilerArguments)
        {
            compilerArguments = "";
            string dir = Path.GetDirectoryName(fileName);
            string fullOutPath = String.Format("{0}\\{1}", dir, outFile);

            switch (shaderType)
            {
                case ShaderType.VERT:
                case ShaderType.TESC:
                case ShaderType.TESE:
                case ShaderType.GEOM:
                case ShaderType.FRAG:
                case ShaderType.COMP:
                    compilerArguments = String.Format("{0} /enable_unbounded_descriptor_tables /E main /T {1} /Fo \"{2}\"", fileName, shaderTarget, fullOutPath);
                    break;
                case ShaderType.RGEN:
                case ShaderType.RCHIT:
                case ShaderType.RMISS:
                case ShaderType.RINT:
                case ShaderType.RAHIT:
                case ShaderType.RCALL:
                    compilerArguments = String.Format("\"{0}\" /T {1} /Fo \"{2}\"", fileName, shaderTarget, fullOutPath);
                    break;

                default:
                    compilerArguments = "INVALID";
                    break;
            }
            
        }

		void DetermineShaderTarget(ShaderType shaderType, string fxc, string dxc, 
                                    out string shaderTarget, out string compilerToUse)
		{
            compilerToUse = "";

            switch (shaderType)
			{
				case ShaderType.VERT:
					shaderTarget = "vs_5_1";
                    compilerToUse = fxc;
                    break;
				case ShaderType.TESC:
					shaderTarget = "hs_5_1";
                    compilerToUse = fxc;
                    break;
				case ShaderType.TESE:
					shaderTarget = "ds_5_1";
                    compilerToUse = fxc;
                    break;
				case ShaderType.GEOM:
					shaderTarget = "gs_5_1";
                    compilerToUse = fxc;
                    break;
				case ShaderType.FRAG:
					shaderTarget = "ps_5_1";
                    compilerToUse = fxc;
                    break;
				case ShaderType.COMP:
					shaderTarget = "cs_5_1";
                    compilerToUse = fxc;
                    break;
                case ShaderType.RGEN:
                case ShaderType.RCHIT:
                case ShaderType.RMISS:
                case ShaderType.RINT:
                case ShaderType.RAHIT:
                case ShaderType.RCALL:
                    shaderTarget = "lib_6_3";
                    compilerToUse = dxc;
                    break;

                default:
					shaderTarget = "INVALID";
					break;
			}
		}

		void FindShaderMacros(String content, ShaderLanguage language, out string[] macros)
		{
			// Format: // USERMACRO: SAMPLE_COUNT [1,2,4]
			String pattern = @"// USERMACRO: ([A-Za-z0-9_]+) \[([0-9]+(,[0-9]+)*)\]";
			Regex regex = new Regex(pattern);
			var matches = regex.Matches(content);

			List<ShaderMacro[]> table = new List<ShaderMacro[]>();
			for (int i = 0; i < matches.Count; ++i)
				table.Add(null);

			for (int i = 0; i < matches.Count; ++i)
			{
				String definition = matches[i].Groups[1].Value;
				String[] values = matches[i].Groups[2].Value.Trim().Split(',');
				table[i] = new ShaderMacro[values.Length];
				for (int j = 0; j < table[i].Length; ++j)
				{
					table[i][j] = new ShaderMacro
					{
						definition = definition,
						value = values[j],
					};
				}
			}

			IEnumerable<string> results = new List<string> { null };
			String format;
			switch (language)
			{
				case ShaderLanguage.HLSL:
					format = " -D {0}={1} ";
					break;
				case ShaderLanguage.VULKAN_GLSL:
					format = " \"-D {0}={1}\" ";
					break;
				default:
					format = "";
					break;
			}
			foreach (var list in table)
			{
				// cross join the current result with each member of the next list
				results = results.SelectMany(o => list.Select(s => o + String.Format(format, s.definition, s.value)));
			}

			macros = results.ToArray();
			if (macros.Length == 1 && macros[0] == null)
				macros[0] = "";
		}

		static bool FindHLSLCompiler(out string compiler, string executable)
		{
			compiler = "";

			if (!ExistsOnPath(executable))
			{
				string[] windowsSDKS =
				{
                    @"C:\Program Files (x86)\Windows Kits\10\bin\10.0.17763.0\x64",
                    @"C:\Program Files (x86)\Windows Kits\10\bin\10.0.16299.0\x64",
					@"C:\Program Files (x86)\Windows Kits\10\bin\10.0.15063.0\x64",
				};
				bool found = false;
				foreach (var v in windowsSDKS)
				{
					if (File.Exists(Path.Combine(v, executable)))
					{
						compiler = Path.Combine(v, executable);
						found = true;
						break;
					}
				}

				return found;
			}
			else
			{
				compiler = executable;
			}

			return true;
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

		private void WriteToOutputWindow(String text)
		{
			IVsOutputWindow outWindow = Package.GetGlobalService(typeof(SVsOutputWindow)) as IVsOutputWindow;

			Guid generalPaneGuid = VSConstants.GUID_OutWindowGeneralPane; // P.S. There's also the GUID_OutWindowDebugPane available.
			outWindow.CreatePane(generalPaneGuid, "Output WIndow", 1, 1);
			outWindow.GetPane(ref generalPaneGuid, out IVsOutputWindowPane generalPane);

			if (generalPane != null)
			{
				generalPane.OutputString(text);
				generalPane.Activate(); // Brings this pane into view
			}
		}
	}
}
