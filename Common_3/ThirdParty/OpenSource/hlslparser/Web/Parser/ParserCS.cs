using System;
using System.IO;
using System.Reflection;
using System.Diagnostics;
using Task = System.Threading.Tasks.Task;
using System.Runtime.InteropServices;

namespace Parser
{

    internal sealed class ParserHelper : IDisposable
    {
        public string FilePath { get; }

        private ParserHelper(string extension)
        {
            FilePath = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString() + extension);
        }

        private ParserHelper(string fileName, string extension)
        {
            FilePath = Path.Combine(Path.GetTempPath(), fileName + extension);
        }


        public static ParserHelper CreateInlcudeFile(string contents, string includeFileName, string extension)
        {
            var result = new ParserHelper(includeFileName, extension);

            File.WriteAllText(result.FilePath, contents);

            return result;
        }



        public static ParserHelper FromShaderCode(string contents)
        {
            var result = new ParserHelper(GetFileExtension());

            File.WriteAllText(result.FilePath, contents);

            return result;
        }

       

        private static string GetFileExtension()
        {
            return ".tmp";
        }

        public void Dispose()
        {
            File.Delete(FilePath);
        }
    }

    public class ParserCS
    {
        /*
        [DllImport("Parser.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.LPStr)]
         public static extern string PARSER([MarshalAs(UnmanagedType.LPStr)]string fileName, [MarshalAs(UnmanagedType.LPStr)]string buffer, int bufferSize, [MarshalAs(UnmanagedType.LPStr)]string entryName, [MarshalAs(UnmanagedType.LPStr)]string shader, [MarshalAs(UnmanagedType.LPStr)]string _language);
        */

        public static void CreateInlcudeFile(string buffer, string fileName, string extension)
        {
            ParserHelper.CreateInlcudeFile(buffer, fileName, extension);
        }


        public static string PARSER(string fileName, string buffer, int bufferSize, string entryName, string shader, string _language)
        {
            using (var tempFileIn = ParserHelper.FromShaderCode(buffer))
            {
                ProcessStartInfo info = null;

                var outputFilePath = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString() + ".tmp");

                // var fullPath = Directory.ge GetCurrentDirectory() + "\\bin\\Release\\netcoreapp2.0\\Parser.exe";

                var fullPath = "wwwroot\\app\\Parser.exe";
                info = new ProcessStartInfo
                {
                    UseShellExecute = false,
                    FileName = fullPath,
                    Arguments = String.Format("{0} {1} {2} -E {3} -Fo {4}", shader, _language, tempFileIn.FilePath, entryName, outputFilePath),
                    RedirectStandardError = true,
                    RedirectStandardOutput = true,
                    CreateNoWindow = true,
                };

                var process = Process.Start(info);

                Task.Run(() => process.StandardOutput.ReadToEndAsync());
                process.WaitForExit();

                tempFileIn.Dispose();

                var result = File.ReadAllText(outputFilePath).Replace("\r\n", "\n");

                File.Delete(outputFilePath);

                return result;

                /*
                using (var tempFileOut = ParserHelper.FromShaderCode(buffer, outputFilePath))
                {
                    //read contents from file
                    File.ReadAllText AllText(tempFileOut.FilePath);
                    return tempFileOut.FilePath;

                }
                */
            } 
        }


        public static string entryName = "main";
        public static string fileName;

        public static string shader = "-vs";
        public static string language = "-glsl";
        public static string languageString = "GLSL";
       

        public static readonly string[] AllShaders =
        {
            "Vertex",
            "Fragment",
            "Hull",
            "Domain",
            "Geometry",
            "Compute"
        };

        static public string shaderString = "Vertex";
    

        public static bool bTranslateAll = true;

        public static string translatedHLSL;
        public static string translatedGLSL;
        public static string translatedMSL;

       

        public static void SetShader(string param)
        {
            //string check = Directory.GetCurrentDirectory() + "\\wwwroot\\dll\\Parser.dll";

            switch (param)
            {
                case "Vertex":
                    shader = "-vs";
                    break;
                case "Fragment":
                    shader = "-fs";
                    break;
                case "Hull":
                    shader = "-hs";
                    break;
                case "Domain":
                    shader = "-ds";
                    break;
                case "Geometry":
                    shader = "-gs";
                    break;
                case "Compute":
                    shader = "-cs";
                    break;
                default:
                    break;
            }
        }

        public static void SetFileName(string param)
        {
            fileName = param;
        }

        public static void SetEntryName(string param)
        {
            entryName = param;
        }

        public static void SetLanguage(string param)
        {
            switch (param)
            {
                case "HLSL":
                    language = "-hlsl";
                    break;
                case "GLSL":
                    language = "-glsl";
                    break;
                case "MSL":
                    language = "-msl";
                    break;
                case "ALL":
                    language = "ALL";
                    break;
                default:
                    break;
            }

            languageString = param;
        }

        public static void Translate(string inputShaderText )
        {
            if (languageString == "ALL")
            {
                translatedHLSL = PARSER(fileName, inputShaderText, inputShaderText.Length, entryName, shader, "-hlsl").Replace("\n", "\r\n");
                translatedGLSL = PARSER(fileName, inputShaderText, inputShaderText.Length, entryName, shader, "-glsl").Replace("\n", "\r\n");
                translatedMSL = PARSER(fileName, inputShaderText, inputShaderText.Length, entryName, shader, "-msl").Replace("\n", "\r\n");                
            }
            else if (languageString == "HLSL")
            {
                translatedHLSL = PARSER(fileName, inputShaderText, inputShaderText.Length, entryName, shader, "-hlsl").Replace("\n", "\r\n");
            }
            else if (languageString == "GLSL")
            {
                translatedGLSL = PARSER(fileName, inputShaderText, inputShaderText.Length, entryName, shader, "-glsl").Replace("\n", "\r\n");
            }
            else if (languageString == "MSL")
            {
                translatedMSL = PARSER(fileName, inputShaderText, inputShaderText.Length, entryName, shader, "-msl").Replace("\n", "\r\n");
            }
        }

        public static string GetResult(string ChosenLanguage)
        {
            switch (ChosenLanguage)
            {
                case "HLSL":
                    return translatedHLSL;
                case "GLSL":
                    return translatedGLSL;
                case "MSL":
                    return translatedMSL;
                default:
                    return "";
            }
        }

        public static string Parsing(string fileNameParam, string bufferParam, string entryNameParam, string shaderParam, string languageParam)
        {
            SetFileName(fileNameParam);
            SetEntryName(entryNameParam);

            SetShader(shaderParam);
            SetLanguage(languageParam);
            Translate(bufferParam);

            return GetResult(languageParam);
        }

        public static string GetStoredResult(string languageParam)
        {
            return GetResult(languageParam);
        }

    }
}
