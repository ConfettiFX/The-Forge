using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Runtime.InteropServices;

namespace HLSLParser
{
    public partial class Form1 : Form
    {        
       
        OpenFileDialog ofd = new OpenFileDialog();
        SaveFileDialog sfd = new SaveFileDialog();

        Input inputWindow;
        Output outputWindow;

        public Form1(string[] args)
        {
            InitializeComponent();
            inputWindow = new Input(this);
            outputWindow = new Output(this);

            inputWindow.SetOutputWindow(outputWindow);
            outputWindow.SetInputWindow(inputWindow);

            sfd.Filter = "vertex shader|*.vert|source vertex shader|*.src_vert|fragment shader|*.frag|source fragment shader|*.src_frag|hull shader|*.tesc|source hull shader|*.src_tesc|domain shader|*.tese|source domain shader|*.src_tese|geometry shader|*.geom|source geometry shader|*.src_geom|compute shader|*.comp|source compute shader|*.src_comp|all files|*.*";
            sfd.FilterIndex = 7;

            sfd.DefaultExt = "vert";
            sfd.AddExtension = true;

            this.StartPosition = FormStartPosition.Manual;

            if (args.Length > 3)
            {
                bTranslateAll = false;

                shader = args[0];
                language = args[1];
                fileName = args[2];
                entryName = args[3];

                OpenInputWindow();
            }

        }

        private void OpenInputWindow()
        {
            if (inputWindow.IsDisposed)
            {
                inputWindow = new Input(this);

                if (outputWindow.IsDisposed)
                {
                    outputWindow = new Output(this);
                }

                inputWindow.SetOutputWindow(outputWindow);
                outputWindow.SetInputWindow(inputWindow);
            }
            else if (outputWindow.IsDisposed)
            {
                outputWindow = new Output(this);

                if (inputWindow.IsDisposed)
                {
                    inputWindow = new Input(this);
                }

                inputWindow.SetOutputWindow(outputWindow);
                outputWindow.SetInputWindow(inputWindow);
            }

            inputWindow.Location = new Point(this.Location.X + 200, this.Location.Y);
            inputWindow.Show();

           

            //inputWindow.fileName[inputWindow.fileName.Length - 1] = textBox1.Text = fileName;

            switch (language)
            {
                case "-hlsl": HLSLradioButton.Checked = true; break;
                case "-glsl": GLSLradioButton.Checked = true; break;
                case "-msl": MSLradioButton.Checked = true; break;
                default: break;
            }



            textBox1.Text = fileName;

            string dirPath = Path.GetDirectoryName(fileName);


            TraverseIncludefiles(fileName, dirPath, includeFileName, includeFileBuffer);



            StreamReader sr = new StreamReader(textBox1.Text);
            string inputData = sr.ReadToEnd();


            includeFileName.Add(fileName);
            //includeFileBuffer.Add(inputData);

            inputWindow.fileNames = includeFileName;
            inputWindow.includeBuffers = includeFileBuffer;

            //StreamReader sr = new StreamReader(textBox1.Text);
            //string inputData = sr.ReadToEnd();

            inputWindow.richTextBox2.Text = inputData;

            char[] splitters = { '.', '\\' };
            string[] tokkens = fileName.Split(splitters);

            if (tokkens[tokkens.Length - 1].Contains("vert"))
            {
                VSradioButton.Checked = true; shader = "-vs";
            }
            else if (tokkens[tokkens.Length - 1].Contains("frag"))
            {
                FSradioButton.Checked = true; shader = "-fs";
            }
            else if (tokkens[tokkens.Length - 1].Contains("tesc"))
            {
                HSradioButton.Checked = true; shader = "-hs";
            }
            else if (tokkens[tokkens.Length - 1].Contains("tese"))
            {
                DSradioButton.Checked = true; shader = "-ds";
            }
            else if (tokkens[tokkens.Length - 1].Contains("geom"))
            {
                GSradioButton.Checked = true; shader = "-gs";
            }
            else if (tokkens[tokkens.Length - 1].Contains("comp"))
            {
                CSradioButton.Checked = true; shader = "-cs";
            }

            inputWindow.Text = inputWindow.label5.Text = "Input from " + shaderString + " \"" + tokkens[tokkens.Length - 2] + "." + tokkens[tokkens.Length - 1] + "\"";
            inputWindow.label5.Update();

            inputWindow.button2.PerformClick();
        }


        bool bTranslateAll = true;


        public string entryName = "main";

        public List<string> includeFileName = new List<string>();
        public List<string> includeFileBuffer = new List<string>();

        public string fileName;

        public string shader = "-vs";
        public string language = "-glsl";
        public string shaderString = "Vertex shader";
        public string languageString = "GLSL";

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        private void radioButton1_CheckedChanged(object sender, EventArgs e)
        {

        }

        public void createOutputWindow()
        {
            outputWindow = new Output(this);

            inputWindow.SetOutputWindow(outputWindow);
            outputWindow.SetInputWindow(inputWindow);
        }

        //fsButton
        private void FSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-fs";
            shaderString = "Fragment shader";
            inputWindow.shaderString = shaderString;
            inputWindow.shader = shader;
            outputWindow.shader = shader;
        }

        //vsButton
        private void VSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-vs";
            shaderString = "Vertex shader";
            inputWindow.shaderString = shaderString;
            inputWindow.shader = shader;
            outputWindow.shader = shader;
        }

        //hsButton
        private void HSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-hs";
            shaderString = "Hull shader";
            inputWindow.shaderString = shaderString;
            inputWindow.shader = shader;
            outputWindow.shader = shader;
        }

        //dsButton
        private void DSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-ds";
            shaderString = "Domain shader";
            inputWindow.shaderString = shaderString;
            inputWindow.shader = shader;
            outputWindow.shader = shader;
        }

        //gsButton
        private void GSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-gs";
            shaderString = "Geometry shader";
            inputWindow.shaderString = shaderString;
            inputWindow.shader = shader;
            outputWindow.shader = shader;
        }

        //csButton
        private void CSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-cs";
            shaderString = "Compute shader";
            inputWindow.shaderString = shaderString;
            inputWindow.shader = shader;
            outputWindow.shader = shader;
        }

        private void groupBox2_Enter(object sender, EventArgs e)
        {

        }

        private void openFileDialog1_FileOk(object sender, CancelEventArgs e)
        {

        }

        private void HLSLradioButton_CheckedChanged(object sender, EventArgs e)
        {
            language = "-hlsl";
            languageString = "HLSL";
            bTranslateAll = false;

            inputWindow.language = language;
            inputWindow.languageString = languageString;
            inputWindow.bTranslateAll = bTranslateAll;
        }

        private void GLSLradioButton_CheckedChanged(object sender, EventArgs e)
        {
            language = "-glsl";
            languageString = "GLSL";
            bTranslateAll = false;

            inputWindow.language = language;
            inputWindow.languageString = languageString;
            inputWindow.bTranslateAll = bTranslateAll;
        }

        private void MSLradioButton_CheckedChanged(object sender, EventArgs e)
        {
            language = "-msl";
            languageString = "MSL";
            bTranslateAll = false;

            inputWindow.language = language;
            inputWindow.languageString = languageString;
            inputWindow.bTranslateAll = bTranslateAll;
        }

        private void AllradioButton_CheckedChanged(object sender, EventArgs e)
        {
            bTranslateAll = AllradioButton.Checked;
            inputWindow.bTranslateAll = bTranslateAll;
        }

        //parser
        private void button2_Click(object sender, EventArgs e)
        {
        }

        private void radioButton2_CheckedChanged(object sender, EventArgs e)
        {

        }

        private void label1_Click(object sender, EventArgs e)
        {

        }

        private void textBox3_TextChanged(object sender, EventArgs e)
        {
           
        }

        private bool TraverseIncludefiles(string path, string dirPath, List<string> includefiles, List<string> includeFileBuffer)
        {
            StreamReader sr = new StreamReader(path);
            string inputData = sr.ReadToEnd();// ReadLine();


            //extract includefiles' name
            char[] splitters0 = { '\"', '<', '>' };
            int index = inputData.LastIndexOf("#include");
            while (index >= 0)
            {

                string[] temp = inputData.Substring(index).Split(splitters0);

                //recursive
                string newfilePath = dirPath + "\\" + temp[1];

                TraverseIncludefiles(newfilePath, dirPath, includefiles, includeFileBuffer);

              

                index = inputData.LastIndexOf("#include", index);

            }

            includefiles.Add(path);
            includeFileBuffer.Add(inputData.Replace("\r", ""));



            return true;

        }

        private void button1_Click(object sender, EventArgs e)
        {
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                if(includeFileName != null)
                    includeFileName.Clear();
                if (includeFileBuffer != null)
                    includeFileBuffer.Clear();
                


                if (inputWindow.IsDisposed)
                {
                    inputWindow = new Input(this);

                    if (outputWindow.IsDisposed)
                    {
                        outputWindow = new Output(this);
                    }

                    inputWindow.SetOutputWindow(outputWindow);
                    outputWindow.SetInputWindow(inputWindow);
                }
                else if (outputWindow.IsDisposed)
                {
                    outputWindow = new Output(this);

                    if (inputWindow.IsDisposed)
                    {
                        inputWindow = new Input(this);
                    }

                    inputWindow.SetOutputWindow(outputWindow);
                    outputWindow.SetInputWindow(inputWindow);
                }



                inputWindow.Location = new Point(this.Location.X + 100, this.Location.Y);
                inputWindow.Show();
                

                if (inputWindow.fileNames != null)
                    inputWindow.fileNames.Clear();
                if (inputWindow.includeBuffers != null)
                    inputWindow.includeBuffers.Clear();


                textBox1.Text = fileName = ofd.FileName;

                string dirPath = Path.GetDirectoryName(fileName);


                TraverseIncludefiles(fileName, dirPath, includeFileName, includeFileBuffer);

               

                StreamReader sr = new StreamReader(textBox1.Text);
                string inputData = sr.ReadToEnd();


                //includeFileName.Add(fileName);
                //includeFileBuffer.Add(inputData);

                inputWindow.fileNames = includeFileName;
                inputWindow.includeBuffers = includeFileBuffer;

                inputWindow.richTextBox2.Text = inputData;

                char[] splitters = { '.', '\\' };
                string[] tokkens = fileName.Split(splitters);

                if (tokkens[tokkens.Length - 1].Contains("vert"))
                {
                    VSradioButton.Checked = true; shader = "-vs";
                }
                else if (tokkens[tokkens.Length - 1].Contains("frag"))
                {
                    FSradioButton.Checked = true; shader = "-fs";
                }
                else if (tokkens[tokkens.Length - 1].Contains("tesc"))
                {
                    HSradioButton.Checked = true; shader = "-hs";
                }
                else if (tokkens[tokkens.Length - 1].Contains("tese"))
                {
                    DSradioButton.Checked = true; shader = "-ds";
                }
                else if (tokkens[tokkens.Length - 1].Contains("geom"))
                {
                    GSradioButton.Checked = true; shader = "-gs";
                }
                else if (tokkens[tokkens.Length - 1].Contains("comp"))
                {
                    CSradioButton.Checked = true; shader = "-cs";
                }

                inputWindow.Text = inputWindow.label5.Text = "Input from " + shaderString + " \"" + tokkens[tokkens.Length - 2] + "." + tokkens[tokkens.Length - 1] + "\"";
                inputWindow.label5.Update();
            }
        }

        //Save
        private void button3_Click(object sender, EventArgs e)
        {
            switch (shader)
            {
                case "-vs": sfd.FilterIndex = 1; break;
                case "-fs": sfd.FilterIndex = 2; break;
                case "-hs": sfd.FilterIndex = 3; break;
                case "-ds": sfd.FilterIndex = 4; break;
                case "-gs": sfd.FilterIndex = 5; break;
                case "-cs": sfd.FilterIndex = 6; break;
                default: break;
            }
            

            if (sfd.ShowDialog() == DialogResult.OK)
            {
                using (Stream s = File.Open(sfd.FileName, FileMode.Create))
                using (StreamWriter sw = new StreamWriter(s))
                {
                    if (outputWindow.Visible)
                        sw.Write(outputWindow.Text);
                }
            }
        }

        private void textBox2_TextChanged(object sender, EventArgs e)
        {
            entryName = textBox2.Text;
            inputWindow.entryName = entryName;
        }

        private void textBox5_TextChanged(object sender, EventArgs e)
        {

        }

        private void label6_Click(object sender, EventArgs e)
        {

        }

        private void label7_Click(object sender, EventArgs e)
        {

        }

        private void label5_Click(object sender, EventArgs e)
        {

        }

        private void label3_Click(object sender, EventArgs e)
        {

        }

        private void richTextBox1_TextChanged(object sender, EventArgs e)
        {

        }

        private void richTextBoxOutputHLSL_TextChanged(object sender, EventArgs e)
        {

        }

        private void richTextBox2_TextChanged(object sender, EventArgs e)
        {

        }
       
       
       
    }
}
