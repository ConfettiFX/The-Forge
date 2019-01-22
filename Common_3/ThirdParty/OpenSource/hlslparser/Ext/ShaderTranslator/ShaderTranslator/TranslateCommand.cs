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
        //static bool USE_INSTALLER = false;

        //For Independent build, it should be activated
        static bool USE_INSTALLER = true;

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
        public enum ShaderType { VERT, TESC, TESE, GEOM, FRAG, COMP };
        string[] shaderExtensions = { "src_vert", "src_tesc", "src_tese", "src_geom", "src_frag", "src_comp" };
        string[] shaderOutputExtensions = { "vert", "tesc", "tese", "geom", "frag", "comp" };

        struct ShaderTranslatorData
        {
            public String arguments;           
            public String shaderFileName;
            public String outputPath;
            public ShaderLanguage language;
            public System.Diagnostics.Process process;
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
                filePath += "\\..\\..\\..\\..\\..\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\UI\\HLSLParser\\HLSLParser\\bin\\Debug\\";
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
                filePath += "\\..\\..\\..\\..\\..\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\UI\\HLSLParser\\HLSLParser\\bin\\Release\\";
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
                filePath += "\\..\\..\\..\\..\\..\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\Debug\\";
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
                filePath += "\\..\\..\\..\\..\\..\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\Release\\";
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

                    if (OldExt == "src_tesc" && sl == ShaderLanguage.MSL)
                    {
                        NewExt = ".vert.tesc.comp";
                    }
                    else if (OldExt == "src_tese" && sl == ShaderLanguage.MSL)
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

        

        private async void MenuItemCallback(object sender, EventArgs e)
        {

            EnvDTE80.DTE2 applicationObject = await this.ServiceProvider.GetServiceAsync(typeof(DTE)) as EnvDTE80.DTE2;

            //String ShaderClass = "HLSL";

            object[] selectedItems = (object[])applicationObject.ToolWindows.SolutionExplorer.SelectedItems;
            var processArray = new List<ShaderTranslatorData>();

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

                        for (int i = 0; i < tokkens.Length - 1; i++)
                            outFile += (tokkens[i] + "\\");

                        //prevent for writing files reading now
                        outFile += "D3D12\\" + FileTokkens[FileTokkens.Length - 1];


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
                        processArray.Add(new ShaderTranslatorData
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

                        for (int i = 0; i < tokkens.Length - 1; i++)
                            outFile += (tokkens[i] + "\\");

                        outFile += "PCVulkan\\" + FileTokkens[FileTokkens.Length - 1];

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
                        processArray.Add(new ShaderTranslatorData
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

                        for (int i = 0; i < tokkens.Length - 1; i++)
                            outFile += (tokkens[i] + "\\");

                        outFile += "Metal\\" + FileTokkens[FileTokkens.Length - 1];

                        outFile = ReplaceExtension(outFile, ShaderLanguage.MSL) + ".metal";

                        String secondaryfileName = "";

                        //vert shader as 2nd shader file
                        if (shaderType == ShaderType.TESC)
                        {
                            //assumes it is using same name
                            secondaryfileName = Path.GetDirectoryName(fileName);
                            secondaryfileName += "\\" + Path.GetFileNameWithoutExtension(fileName) + ".svert";
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
                        processArray.Add(new ShaderTranslatorData
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
                }
            }
            
            for (int i = 0; i < processArray.Count; ++i)
            {
                //processArray[i].process.StandardError.ReadToEnd();
                var resut = Task.Run(() => processArray[i].process.StandardOutput.ReadToEndAsync());
                processArray[i].process.WaitForExit();

                if (processArray[i].process.ExitCode != 0)
                {
                    if (processArray[i].language == ShaderLanguage.HLSL)
                    {
                        WriteToOutputWindow(String.Format("Failure : Fail to translate {0} ({1}) \n", processArray[i].shaderFileName, "HLSL"));
                    }
                    else if (processArray[i].language == ShaderLanguage.GLSL)
                    {
                        WriteToOutputWindow(String.Format("Failure : Fail to translate {0} ({1}) \n", processArray[i].shaderFileName, "GLSL"));
                    }
                    else if (processArray[i].language == ShaderLanguage.MSL)
                    {
                        WriteToOutputWindow(String.Format("Failure : Fail to translate {0} ({1}) \n", processArray[i].shaderFileName, "MSL"));
                    }

                    //if it is failed, pop the UI
                    ProcessStartInfo startInfo = new ProcessStartInfo("");

                    startInfo.UseShellExecute = false;

                    //Temporarily, it assumes the shader is in the unit test folder
                    string filePath = Path.GetDirectoryName(processArray[i].shaderFileName);

                    filePath = GetShaderTranslatorPath(filePath);

                    if (filePath == null)
                        return;


                    filePath += "ConfettiShaderTranslator.exe";

                    //WriteToOutputWindow(String.Format("Directory : {0} \n", filePath));



                    startInfo.FileName = filePath;

                    startInfo.Arguments = processArray[i].arguments;
                    startInfo.RedirectStandardError = true;
                    startInfo.RedirectStandardOutput = true;
                    startInfo.CreateNoWindow = true;

                    System.Diagnostics.Process.Start(startInfo);

                    return;
                }
                else
                {                    
                    //WriteToOutputWindow(String.Format("Success : Shader {0} is translated successfully\n", processArray[i].shaderFileName));

                    if (processArray[i].language == ShaderLanguage.HLSL)
                    {
                        WriteToOutputWindow(String.Format("Success : Shader {0} ({1}) is generated successfully, Path : {2}\n", processArray[i].shaderFileName, "HLSL", processArray[i].outputPath));
                    }
                    else if (processArray[i].language == ShaderLanguage.GLSL)
                    {
                        WriteToOutputWindow(String.Format("Success : Shader {0} ({1}) is generated successfully, Path : {2}\n", processArray[i].shaderFileName, "GLSL", processArray[i].outputPath));
                    }
                    else if (processArray[i].language == ShaderLanguage.MSL)
                    {
                        WriteToOutputWindow(String.Format("Success : Shader {0} ({1}) is generated successfully, Path : {2}\n", processArray[i].shaderFileName, "MSL", processArray[i].outputPath));
                    }
                }
            }
        }
    }

}
