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
    internal sealed class MSLCommand
    {
        /// <summary>
        /// Command ID.
        /// </summary>
        public const int CommandId = 256;

        /// <summary>
        /// Command menu group (command set GUID).
        /// </summary>
        public static readonly Guid CommandSet = new Guid("ebaf8cc2-cdf4-4fd6-865a-af5ca808d16e");

        /// <summary>
        /// VS Package that provides this command, not null.
        /// </summary>
        private readonly AsyncPackage package;

        /// <summary>
        /// Initializes a new instance of the <see cref="MSLCommand"/> class.
        /// Adds our command handlers for menu (commands must exist in the command table file)
        /// </summary>
        /// <param name="package">Owner package, not null.</param>
        /// <param name="commandService">Command service to add command to, not null.</param>
        private MSLCommand(AsyncPackage package, OleMenuCommandService commandService)
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
        public static MSLCommand Instance
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
            // Verify the current thread is the UI thread - the call to AddCommand in MSLCommand's constructor requires
            // the UI thread.
            ThreadHelper.ThrowIfNotOnUIThread();

            OleMenuCommandService commandService = await package.GetServiceAsync((typeof(IMenuCommandService))) as OleMenuCommandService;
            Instance = new MSLCommand(package, commandService);
        }

        enum ShaderLanguage { HLSL, GLSL, MSL };
        public enum ShaderType { VERT, TESC, TESE, GEOM, FRAG, COMP };
        string[] shaderExtensions = { "vert", "tesc", "tese", "geom", "frag", "comp" };

        struct ShaderTranslatorData
        {
            public String fileName;
            public String outputName;
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
            string title = "MSLCommand";

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

        static bool FindTranslator(out string cst)
        {
            String cstExecutable = "ConfettiShaderTranslator.exe";
            cst = "";

            if (!ExistsOnPath(cstExecutable))
            {
                string[] cstPath =
                {
#if DEBUG
                    @"C:\Confetti\TheForge\Common_3\ThirdParty\OpenSource\hlslparser\Debug",
                    @"..\..\..\Common_3\ThirdParty\OpenSource\hlslparser\Debug",
#else
                    @"C:\Confetti\TheForge\Common_3\ThirdParty\OpenSource\hlslparser\Release",
                    @"..\..\..\Common_3\ThirdParty\OpenSource\hlslparser\Release",
#endif
                };
                bool found = false;
                foreach (var v in cstPath)
                {
                    if (File.Exists(Path.Combine(v, cstExecutable)))
                    {
                        cst = Path.Combine(v, cstExecutable);
                        found = true;
                        break;
                    }
                }

                return found;
            }
            else
            {
                cst = cstExecutable;
            }

            return true;
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

        private async void MenuItemCallback(object sender, EventArgs e)
        {

            EnvDTE80.DTE2 applicationObject = await this.ServiceProvider.GetServiceAsync(typeof(DTE)) as EnvDTE80.DTE2;



            object[] selectedItems = (object[])applicationObject.ToolWindows.SolutionExplorer.SelectedItems;
            var processArray = new List<ShaderTranslatorData>();

            String ShaderClass = "MSL";

            FindTranslator(out string cst);

            foreach (EnvDTE.UIHierarchyItem selectedUIHierarchyItem in selectedItems)
            {
                if (selectedUIHierarchyItem.Object is EnvDTE.ProjectItem)
                {
                    EnvDTE.ProjectItem item = selectedUIHierarchyItem.Object as EnvDTE.ProjectItem;
                    int size = item.FileCount;
                    string fileName = item.FileNames[0];
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



                        if (tokkens[tokkens.Length - 1] == "D3D12")
                        {
                            outFile = "";

                            for (int i = 0; i < tokkens.Length - 1; i++)
                                outFile += (tokkens[i] + "\\");

                            outFile += "Metal\\" + FileTokkens[FileTokkens.Length - 1] + ".metal";
                        }
                        else
                        {
                            WriteToOutputWindow(String.Format("Failure : Original Shader should be from {0}\n", "HLSL"));
                            continue;
                        }

                        if (cst == "")
                        {
                            WriteToOutputWindow("Cannot find Confetti Shader Compiler.");
                            continue;
                        }

                        ProcessStartInfo info = null;

                        info = new ProcessStartInfo
                        {
                            UseShellExecute = false,
                            FileName = cst,
                            Arguments = String.Format("-{0} -msl {1} main {2}", shaderArgument, fileName, outFile),
                            RedirectStandardError = true,
                            RedirectStandardOutput = true,
                            CreateNoWindow = true,
                        };

                        String prevArgs = info.Arguments;
                        processArray.Add(new ShaderTranslatorData
                        {
                            fileName = fileName,
                            outputName = Path.GetFileName(outFile),
                            language = ShaderLanguage.MSL,
                            process = System.Diagnostics.Process.Start(info),
                        }
                        );
                        info.Arguments = prevArgs;
                    }
                }
            }


            for (int i = 0; i < processArray.Count; ++i)
            {
                var resut = Task.Run(() => processArray[i].process.StandardOutput.ReadToEndAsync());
                processArray[i].process.WaitForExit();

                if (processArray[i].process.ExitCode != 0)
                {
                    WriteToOutputWindow(String.Format("Failure : Shader {0} \n", Path.GetFileName(processArray[i].fileName)));

                    //if it is failed, pop the UI
                    ProcessStartInfo startInfo = new ProcessStartInfo("");

                    startInfo.UseShellExecute = false;

                    //Temporarily, it assumes the shader is in the unit test folder
#if DEBUG
                    string filePath = Path.GetDirectoryName(processArray[i].fileName) + "\\..\\..\\..\\..\\..\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\UI\\HLSLParser\\HLSLParser\\bin\\Debug\\" + "HLSLParser.exe";
#else
                    string filePath = Path.GetDirectoryName(processArray[i].fileName) + "\\..\\..\\..\\..\\..\\Common_3\\ThirdParty\\OpenSource\\hlslparser\\UI\\HLSLParser\\HLSLParser\\bin\\Release\\" + "HLSLParser.exe";
#endif

                    //WriteToOutputWindow(String.Format("Directory : {0} \n", filePath));

                    startInfo.FileName = filePath;

                    startInfo.Arguments = String.Format("-{0} -msl {1} main {2}", processArray[i].language, processArray[i].fileName, processArray[i].outputName);
                    startInfo.RedirectStandardError = true;
                    startInfo.RedirectStandardOutput = true;
                    startInfo.CreateNoWindow = true;

                    System.Diagnostics.Process.Start(startInfo);
                    /*
                    RegistryKey key = Registry.LocalMachine.OpenSubKey("Software\\Wow6432Node\\Confetti");
                    String path = "";

                    if (key != null)
                    {
                        Object o = key.GetValue("CST_PATHs");
                        if (o != null)
                            path = o as String;
                    }

                    if (path == "")
                    {
                        WriteToOutputWindow(String.Format("Error : Cannot find the Confetti Shader Translator!"));
                    }
                    else
                    {
                        startInfo.FileName = path + "HLSLParser.exe";

                        startInfo.Arguments = String.Format("-{0} -msl {1} main {2}", processArray[i].language, processArray[i].fileName, processArray[i].outputName);
                        startInfo.RedirectStandardError = true;
                        startInfo.RedirectStandardOutput = true;
                        startInfo.CreateNoWindow = true;

                        System.Diagnostics.Process.Start(startInfo);
                    }
                    */
                }
                else
                {
                    WriteToOutputWindow(String.Format("Success : Shader {0} is translated successfully\n", Path.GetFileName(processArray[i].fileName)));
                    WriteToOutputWindow(String.Format("Success : Shader {0} ({1}) is generated successfully\n", Path.GetFileName(processArray[i].outputName), ShaderClass));
                }
            }
        }
    }
}
