using System;
using System.Collections.Generic;
using System.ComponentModel.Design;
using System.Globalization;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Task = System.Threading.Tasks.Task;
using System.IO;
using EnvDTE;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Linq;
using Microsoft.Win32;

namespace ShaderTranslator
{
    

    /// <summary>
    /// Command handler
    /// </summary>
    internal sealed class TranslateCommand
    {
        //For Independent build, it should be activated
        //static bool USE_INSTALLER = true;
        static bool USE_INSTALLER = false;

        /// <summary>
        /// Command ID.
        /// </summary>
        public const int CommandId = 256;

        /// <summary>
        /// Command menu group (command set GUID).
        /// </summary>
        public static readonly Guid CommandSet = new Guid("30c752c6-58b5-4ae7-80d8-2bc2b0d15b02");

        /// <summary>
        /// VS Package that provides this command, not null.
        /// </summary>
        private readonly AsyncPackage package;

        /// <summary>
        /// Initializes a new instance of the <see cref="TranslateCommand"/> class.
        /// Adds our command handlers for menu (commands must exist in the command table file)
        /// </summary>
        /// <param name="package">Owner package, not null.</param>
        /// <param name="commandService">Command service to add command to, not null.</param>
        private TranslateCommand(AsyncPackage package, OleMenuCommandService commandService)
        {
            this.package = package ?? throw new ArgumentNullException(nameof(package));
            commandService = commandService ?? throw new ArgumentNullException(nameof(commandService));

            var menuCommandID = new CommandID(CommandSet, CommandId);
            var menuItem = new MenuCommand(this.MenuItemCallback, menuCommandID);
            commandService.AddCommand(menuItem);
        }

        /// <summary>
        /// Gets the instance of the command.
        /// </summary>
        public static TranslateCommand Instance
        {
            get;
            private set;
        }

        /// <summary>
        /// Gets the service provider from the owner package.
        /// </summary>
        private Microsoft.VisualStudio.Shell.IAsyncServiceProvider ServiceProvider
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
        public static async Task InitializeAsync(AsyncPackage package)
        {
            // Verify the current thread is the UI thread - the call to AddCommand in TranslateCommand's constructor requires
            // the UI thread.
            ThreadHelper.ThrowIfNotOnUIThread();

            OleMenuCommandService commandService = await package.GetServiceAsync((typeof(IMenuCommandService))) as OleMenuCommandService;
            Instance = new TranslateCommand(package, commandService);
        }

        public enum ShaderLanguage { HLSL, GLSL, MSL };
        public enum ShaderType { VERT, TESC, TESE, GEOM, FRAG, COMP, RGEN, RCHIT, RMISS, RINT, RAHIT, RCALL };
    
        string[] shaderExtensions = { "vert", "tesc", "tese", "geom", "frag", "comp", "rgen", "rchit", "rmiss", "rint", "rahit", "rcall" };
        string[] shaderOutputExtensions = { "vert", "tesc", "tese", "geom", "frag", "comp", "rgen", "rchit", "rmiss", "rint", "rahit", "rcall" };

        struct ShaderTranslatorData
        {
            public String arguments;           
            public String shaderFileName;
            public String outputPath;
            public ShaderLanguage language;
            public System.Diagnostics.Process process;
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
        private void Execute(object sender, EventArgs e)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            string message = string.Format(CultureInfo.CurrentCulture, "Inside {0}.MenuItemCallback()", this.GetType().FullName);
            string title = "TranslateCommand";

            // Show a message box to prove we were here
            VsShellUtilities.ShowMessageBox(
                this.package,
                message,
                title,
                OLEMSGICON.OLEMSGICON_INFO,
                OLEMSGBUTTON.OLEMSGBUTTON_OK,
                OLEMSGDEFBUTTON.OLEMSGDEFBUTTON_FIRST);
        }

        void DetermineShaderLanguage(String content, out ShaderLanguage language)
        {
            String vulkanPattern = "#version \\d+( core)?";
            Regex regex = new Regex(vulkanPattern);
            if (regex.IsMatch(content))
            {
                language = ShaderLanguage.GLSL;
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
                case ShaderLanguage.GLSL:
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

        bool IsShaderFile(String fileName, out ShaderType shaderType, out String ext)
        {
            for (int i = 0; i < shaderExtensions.Length; ++i)
            {
                if (fileName.EndsWith(shaderExtensions[i]))
                {
                    ext = shaderExtensions[i];
                    shaderType = (ShaderType)i;
                    return true;
                }
            }

            ext = "";

            shaderType = (ShaderType)(-1);
            return false;
        }


        private string GetShaderTranslatorPath(string filePath)
        {
#if DEBUG
            if (USE_INSTALLER)
            {
                var hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
                RegistryKey key64 = hklm.OpenSubKey("SOFTWARE\\Confetti");

                if (key64 != null)
                {
                    Object o = key64.GetValue("CST_PATHs");
                    if (o != null)
                    {
                        filePath = o as String;
                        return filePath;
                    }
                    else
                    {
                        WriteToOutputWindow(String.Format("Failure : Cannot find the path of Confetti Shader Translator from registry"));
                        return null;
                    }
                }
                else
                {
                    WriteToOutputWindow(String.Format("Failure : Cannot find the path of Confetti Shader Translator from registry"));
                    return null;
                }
            }
            else
            {
                filePath += "\\..\\..\\..\\Engine.Native\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\UI\\HLSLParser\\HLSLParser\\bin\\Debug\\";
                return filePath;
            }
#else
            if (USE_INSTALLER)
            {
                var hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
                RegistryKey key64 = hklm.OpenSubKey("SOFTWARE\\Confetti");

                if (key64 != null)
                {
                    Object o = key64.GetValue("CST_PATHs");
                    if (o != null)
                    {
                        filePath = o as String;
                        return filePath;
                    }
                    else
                    {
                        WriteToOutputWindow(String.Format("Failure : Cannot find the path of Confetti Shader Translator from registry"));
                        return null;
                    }
                }
                else
                    {
                        WriteToOutputWindow(String.Format("Failure : Cannot find the path of Confetti Shader Translator from registry"));
                        return null;
                    }
            }
            else
            {
                filePath += "\\..\\..\\..\\Engine.Native\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\UI\\HLSLParser\\HLSLParser\\bin\\Release\\";
                return filePath;
            }
#endif        
        }


        private string GetParserPath(string filePath)
        {
#if DEBUG
            if (USE_INSTALLER)
            {

                var hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
                RegistryKey key64 = hklm.OpenSubKey("SOFTWARE\\Confetti");

                if (key64 != null)
                {
                    Object o = key64.GetValue("CST_PATHs");
                    if (o != null)
                    {
                        filePath = o as String;
                        return filePath;
                    }
                    else
                    {
                        WriteToOutputWindow(String.Format("Failure : Cannot find the path of Confetti Shader Translator from registry"));
                        return null;
                    }
                }
                else
                {
                    WriteToOutputWindow(String.Format("Failure : Cannot find the path of Confetti Shader Translator from registry"));
                    return null;
                }
            }
            else
            {
                filePath += "\\..\\..\\..\\Engine.Native\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\Debug\\";
                return filePath;
            }
#else
            if (USE_INSTALLER)
            {
                var hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
                RegistryKey key64 = hklm.OpenSubKey("SOFTWARE\\Confetti");

                if (key64 != null)
                {
                    Object o = key64.GetValue("CST_PATHs");
                    if (o != null)
                    {
                        filePath = o as String;
                        return filePath;
                    }
                    else
                    {
                        WriteToOutputWindow(String.Format("Failure : Cannot find the path of Confetti Shader Translator from registry"));
                        return null;
                    }
                }
                else
                {
                    WriteToOutputWindow(String.Format("Failure : Cannot find the path of Confetti Shader Translator from registry"));
                    return null;
                }
            }
            else
            {
                filePath += "\\..\\..\\..\\Engine.Native\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\Release\\";
                return filePath;
            }
#endif

        }

        private bool FindTranslator(out string cst, string fileName)
        {
            String cstExecutable = "Parser.exe";
            cst = "";

            if (!ExistsOnPath(cstExecutable))
            {
                string currentDir = Path.GetDirectoryName(fileName);
                string FilePath = GetParserPath(currentDir);

                FilePath += cstExecutable;

               if (File.Exists(FilePath))
               {
                  cst = FilePath;
                  return true;
               }
               
               return false;
            }
            else
            {
                cst = cstExecutable;
            }

            return false;
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

        private String ReplaceExtension(String outFile, ShaderLanguage sl)
        {
            for (int i = 0; i < shaderExtensions.Length; i++)
            {
                String OldExt = Path.GetExtension(outFile);

                OldExt = OldExt.Remove(0, 1);

                if (OldExt == shaderExtensions[i])
                {
                    String NewExt;

                    if (OldExt == "tesc" && sl == ShaderLanguage.MSL)
                    {
                        NewExt = ".vert.tesc.comp";
                    }
                    else if (OldExt == "tese" && sl == ShaderLanguage.MSL)
                    {
                        NewExt = ".tese.vert";
                    }
                    else
                     NewExt = "." + shaderOutputExtensions[i];

                    OldExt = "." + OldExt;

                    return outFile.Replace(OldExt, NewExt);
                }
            }

            return outFile;
        }

        void CompileShader(string fileName, string fxc, string dxc)
        {
            var processCompileArray = new List<ShaderCompileData>();

            if (IsShaderFile(fileName, out ShaderType shaderType, out String ext))
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
                                    return;
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
                        case ShaderLanguage.GLSL:
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
                        String macroFormat = String.Format("_{0}_{1}", processCompileArray.Count, outFile);

                        info.Arguments += macros[i];
                        info.Arguments = info.Arguments.Replace(outFile, macroFormat);
                        WriteToOutputWindow(String.Format("Compiling {0}:\n\t {1} {2}\n", fileName, info.FileName, info.Arguments));
                        processCompileArray.Add(new ShaderCompileData
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




            for (int i = 0; i < processCompileArray.Count; ++i)
            {
                // glslangValidator error message format
                /*
				 * ERROR: C:\Users\agent47\Documents\Experimental\VisualStudio\ConsoleApp1\ConsoleApp1\skybox.frag:24: '' :  syntax error, unexpected IDENTIFIER
				 */

                String SL = processCompileArray[i].language == ShaderLanguage.GLSL ? "GLSL" : "HLSL";

                String errorLog = processCompileArray[i].language == ShaderLanguage.GLSL ?
                    processCompileArray[i].process.StandardOutput.ReadToEnd() : processCompileArray[i].process.StandardError.ReadToEnd();
                processCompileArray[i].process.WaitForExit();
                if (processCompileArray[i].process.ExitCode != 0)
                {
                    // For Vulkan format the error message so user can double click on it in output window to get to exact line with error
                    if (processCompileArray[i].language == ShaderLanguage.GLSL)
                    {
                        errorLog = errorLog.Replace("ERROR: ", "");
                        String pattern = "([A-Za-z.0-9]+):(\\d+):";
                        Regex regex = new Regex(pattern);
                        String replacement = String.Format("$1($2):");
                        String formattedError = regex.Replace(errorLog, replacement);
                        errorLog = formattedError;
                    }
                    WriteToOutputWindow(String.Format("Failure : Shader [{0}] {1} ({2}) \n{3}", Path.GetFileName(processCompileArray[i].fileName), processCompileArray[i].macros, SL, errorLog));
                }
                else
                {
                    WriteToOutputWindow(String.Format("Success : Shader [{0}] {1} ({2}) compiled successfully\n", Path.GetFileName(processCompileArray[i].fileName), processCompileArray[i].macros, SL));                    
                }

                // Cleanup
                if (processCompileArray[i].language == ShaderLanguage.GLSL)
                {
                    File.Delete(String.Format("SPIRV_OUTPUT_{0}.bin", i));
                }
                File.Delete(processCompileArray[i].outFileName);
            }
        }

        private async void MenuItemCallback(object sender, EventArgs e)
        {

            EnvDTE80.DTE2 applicationObject = await this.ServiceProvider.GetServiceAsync(typeof(DTE)) as EnvDTE80.DTE2;

            //String ShaderClass = "HLSL";

            object[] selectedItems = (object[])applicationObject.ToolWindows.SolutionExplorer.SelectedItems;
            var processTranslatorArray = new List<ShaderTranslatorData>();
            

            FindHLSLCompiler(out string fxc, "fxc.exe");
            FindHLSLCompiler(out string dxc, "dxc.exe");

            foreach (EnvDTE.UIHierarchyItem selectedUIHierarchyItem in selectedItems)
            {
                if (selectedUIHierarchyItem.Object is EnvDTE.ProjectItem)
                {
                    EnvDTE.ProjectItem item = selectedUIHierarchyItem.Object as EnvDTE.ProjectItem;
                    int size = item.FileCount;
                    string fileName = item.FileNames[0];

                    if (!FindTranslator(out string cst, fileName))
                    {
                        WriteToOutputWindow("Failure : Cannot find Confetti Shader Compiler\n");
                        WriteToOutputWindow(String.Format("Path : {0}\n", cst));
                        return;
                    }

                    if (IsShaderFile(fileName, out ShaderType shaderType, out String ext))
                    {
                        String shaderArgument = "";
                        switch (shaderType)
                        {
                            case ShaderType.VERT:
                                shaderArgument = "vs";
                                break;
                            case ShaderType.TESC:
                                shaderArgument = "hs";
                                break;
                            case ShaderType.TESE:
                                shaderArgument = "ds";
                                break;
                            case ShaderType.GEOM:
                                shaderArgument = "gs";
                                break;
                            case ShaderType.FRAG:
                                shaderArgument = "fs";
                                break;
                            case ShaderType.COMP:
                                shaderArgument = "cs";
                                break;
                            default:
                                WriteToOutputWindow(String.Format("Failure : Unclassified Shader\n"));
                                return;
                        }

                        String outFile = Path.GetFileName(fileName);

                        String[] FileTokkens = fileName.Split('\\');

                        String dirName = Path.GetDirectoryName(fileName);

                        String[] tokkens = dirName.Split('\\');


                        /*
                        if (Path.GetExtension(fileName) != )
                        {
                            WriteToOutputWindow(String.Format("Failure : Original Shader should be from HLSL\n"));
                            continue;
                        }
                       */

                        ProcessStartInfo info = null;

                        //HLSL
                        outFile = "";

                        for (int i = 0; i < tokkens.Length; i++)
                            outFile += (tokkens[i] + "\\");

                        //prevent for writing files reading now
                        outFile += "HLSL\\" + FileTokkens[FileTokkens.Length - 1];


                        outFile = ReplaceExtension(outFile, ShaderLanguage.HLSL);
                        

                        info = new ProcessStartInfo
                        {
                            UseShellExecute = false,
                            FileName = cst,
                            Arguments = String.Format("-{0} -hlsl {1} main {2}", shaderArgument, fileName, outFile),
                            RedirectStandardError = true,
                            RedirectStandardOutput = true,
                            CreateNoWindow = true,
                        };


                        String prevArgs = info.Arguments;
                        processTranslatorArray.Add(new ShaderTranslatorData
                        {
                            arguments = info.Arguments,
                            shaderFileName = fileName,
                            language = ShaderLanguage.HLSL,
                            outputPath = outFile,
                            process = System.Diagnostics.Process.Start(info),
                        }
                        );
                        info.Arguments = prevArgs;

                        //GLSL
                        outFile = "";

                        for (int i = 0; i < tokkens.Length; i++)
                            outFile += (tokkens[i] + "\\");

                        outFile += "GLSL\\" + FileTokkens[FileTokkens.Length - 1];

                        outFile = ReplaceExtension(outFile, ShaderLanguage.GLSL);

                        info = new ProcessStartInfo
                        {
                            UseShellExecute = false,
                            FileName = cst,
                            Arguments = String.Format("-{0} -glsl {1} main {2}", shaderArgument, fileName, outFile),
                            RedirectStandardError = true,
                            RedirectStandardOutput = true,
                            CreateNoWindow = true,
                        };


                        prevArgs = info.Arguments;
                        processTranslatorArray.Add(new ShaderTranslatorData
                        {
                            arguments = info.Arguments,
                            shaderFileName = fileName,
                            language = ShaderLanguage.GLSL,
                            outputPath = outFile,
                            process = System.Diagnostics.Process.Start(info),
                        }
                        );
                        info.Arguments = prevArgs;

                        //MSL
                        outFile = "";

                        for (int i = 0; i < tokkens.Length; i++)
                            outFile += (tokkens[i] + "\\");

                        outFile += "MSL\\" + FileTokkens[FileTokkens.Length - 1];

                        outFile = ReplaceExtension(outFile, ShaderLanguage.MSL) + ".metal";

                        String secondaryfileName = "";

                        //vert shader as 2nd shader file
                        if (shaderType == ShaderType.TESC)
                        {
                            //assumes it is using same name
                            secondaryfileName = Path.GetDirectoryName(fileName);
                            secondaryfileName += "\\" + Path.GetFileNameWithoutExtension(fileName) + ".vert";
                        }

                        info = new ProcessStartInfo
                        {
                            UseShellExecute = false,
                            FileName = cst,
                            Arguments = String.Format("-{0} -msl {1} main {2} {3} main", shaderArgument, fileName, outFile, secondaryfileName),
                            RedirectStandardError = true,
                            RedirectStandardOutput = true,
                            CreateNoWindow = true,
                        };


                        prevArgs = info.Arguments;
                        processTranslatorArray.Add(new ShaderTranslatorData
                            {
                                arguments = info.Arguments,
                                shaderFileName = fileName,
                                language = ShaderLanguage.MSL,
                                outputPath = outFile,
                                process = System.Diagnostics.Process.Start(info),
                            }
                        );
                        info.Arguments = prevArgs;


                    }
                    else
                    {
                        WriteToOutputWindow(String.Format("Not supported : {0} Supported extensions are {1}\n",
                            Path.GetFileName(fileName), String.Join(", ", shaderExtensions)));
                    }

                    WriteToOutputWindow(String.Format("[[[ Translating [{0}] get started! ]]]\n", Path.GetFileName(fileName)));

                    for (int i = 0; i < processTranslatorArray.Count; ++i)
                    {
                        //processArray[i].process.StandardError.ReadToEnd();
                        var resut = Task.Run(() => processTranslatorArray[i].process.StandardOutput.ReadToEndAsync());
                        processTranslatorArray[i].process.WaitForExit();

                        if (processTranslatorArray[i].process.ExitCode != 0)
                        {
                            if (processTranslatorArray[i].language == ShaderLanguage.HLSL)
                            {
                                WriteToOutputWindow(String.Format("Failure : Fail to translate {0} ({1}) \n", processTranslatorArray[i].shaderFileName, "HLSL"));
                            }
                            else if (processTranslatorArray[i].language == ShaderLanguage.GLSL)
                            {
                                WriteToOutputWindow(String.Format("Failure : Fail to translate {0} ({1}) \n", processTranslatorArray[i].shaderFileName, "GLSL"));
                            }
                            else if (processTranslatorArray[i].language == ShaderLanguage.MSL)
                            {
                                WriteToOutputWindow(String.Format("Failure : Fail to translate {0} ({1}) \n", processTranslatorArray[i].shaderFileName, "MSL"));
                            }

                            continue;
                        }
                        else
                        {
                            if (processTranslatorArray[i].language == ShaderLanguage.HLSL)
                            {
                                WriteToOutputWindow(String.Format("Success : Shader [{0} ({1})] is generated successfully, Path : {2}\n", Path.GetFileName(processTranslatorArray[i].shaderFileName), "HLSL", processTranslatorArray[i].outputPath));
                                CompileShader(processTranslatorArray[i].outputPath, fxc, dxc);
                            }
                            else if (processTranslatorArray[i].language == ShaderLanguage.GLSL)
                            {
                                WriteToOutputWindow(String.Format("Success : Shader [{0} ({1})] is generated successfully, Path : {2}\n", Path.GetFileName(processTranslatorArray[i].shaderFileName), "GLSL", processTranslatorArray[i].outputPath));
                                CompileShader(processTranslatorArray[i].outputPath, fxc, dxc);
                            }
                            else if (processTranslatorArray[i].language == ShaderLanguage.MSL)
                            {
                                WriteToOutputWindow(String.Format("Success : Shader [{0} ({1})] is generated successfully, Path : {2}\n", Path.GetFileName(processTranslatorArray[i].shaderFileName), "MSL", processTranslatorArray[i].outputPath));
                                //CompileShader(processTranslatorArray[i].outputPath, fxc, dxc);
                            }
                        }
                    }

                    WriteToOutputWindow(String.Format("[[[ Translating [{0}] is done! ]]]\n", Path.GetFileName(fileName)));

                    processTranslatorArray.Clear();

                }
            }


            

            
        }
    }

}
