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
    public partial class Input : Form
    {
        [DllImport("Parser.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        public static extern string PARSER([MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.LPStr)] string [] _fileName, [MarshalAs(UnmanagedType.LPStr)] string buffer, int bufferSize, [MarshalAs(UnmanagedType.LPStr)] string entryName,
            [MarshalAs(UnmanagedType.LPStr)] string shader, [MarshalAs(UnmanagedType.LPStr)] string _language, [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.LPStr)] string[] _bufferForInlcuded, int _includedCounter);

        Form1 parentForm;
        Output outputWindow;

        public string entryName = "main";

        public List<string> fileNames;
        public List<string> includeBuffers;

        public string shader = "-vs";
        public string language = "-glsl";
        public string shaderString = "Vertex shader";
        public string languageString = "GLSL";

        //public string[] bufferForInlcuded;
        //public int includedCounter;

        public bool bTranslateAll = true;

        public string translatedHLSL;
        public string translatedGLSL;
        public string translatedMSL;

        public Input(Form1 paramForm)
        {
            parentForm = paramForm;
            InitializeComponent();

            this.StartPosition = FormStartPosition.Manual;

            richTextBox1.Text = "/*\r\n * Copyright (c) 2018-2019 Confetti Interactive Inc.\r\n * \r\n * This file is part of The-Forge\r\n * (see https://github.com/ConfettiFX/The-Forge). \r\n *\r\n * Licensed to the Apache Software Foundation (ASF) under one\r\n * or more contributor license agreements.  See the NOTICE file\r\n * distributed with this work for additional information\r\n * regarding copyright ownership.  The ASF licenses this file\r\n * to you under the Apache License, Version 2.0 (the\r\n * \"License\") you may not use this file except in compliance\r\n * with the License.  You may obtain a copy of the License at\r\n *\r\n *   http://www.apache.org/licenses/LICENSE-2.0\r\n *\r\n * Unless required by applicable law or agreed to in writing,\r\n * software distributed under the License is distributed on an\r\n * \"AS IS\" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY\r\n * KIND, either express or implied.  See the License for the\r\n * specific language governing permissions and limitations\r\n * under the License.\r\n*/\r\n";
            richTextBox1.Update();
        }

        public void SetOutputWindow(Output paramForm)
        {
            outputWindow = paramForm;
        }

        private void splitContainer1_SplitterMoved(object sender, SplitterEventArgs e)
        {

        }

        private void richTextBox1_TextChanged(object sender, EventArgs e)
        {

        }

        private void button2_Click(object sender, EventArgs e)
        {
            int includeCounter = includeBuffers.Count - 1;

            /*
            string[] fileName = new string[fileNames.Count];


            for (int i = 0; i < fileNames.Count; i++)
            {
                fileName[i] = fileNames[i];
            }

            string[] bufferForInlcuded = new string[includeCounter];

            for (int i = 0; i < includeCounter; i++)
            {
                bufferForInlcuded[i] = includeBuffers[i];
            }
            */


            if (richTextBox2.Text != "")
            {
                //send request to recreate
                if(outputWindow.IsDisposed)
                {
                    parentForm.createOutputWindow();
                }

                outputWindow.Location = new Point(this.Location.X + 620, this.Location.Y);
                outputWindow.Show();

                richTextBox2.Text.Replace("\r", "");

                if (bTranslateAll)
                {
                    translatedHLSL = PARSER(fileNames.ToArray(), richTextBox2.Text, richTextBox2.TextLength, entryName, shader, "-hlsl", includeBuffers.ToArray(), includeCounter).Replace("\n", "\r\n");
                    outputWindow.richTextBoxOutput.Text = richTextBox1.Text + "\r\n" + translatedHLSL;

                    if (outputWindow.richTextBoxOutput.Text == null)
                        outputWindow.richTextBoxOutput.Text = "Failed to translate";
                    
                    translatedGLSL = PARSER(fileNames.ToArray(), richTextBox2.Text, richTextBox2.TextLength, entryName, shader, "-glsl", includeBuffers.ToArray(), includeCounter).Replace("\n", "\r\n");

                    if (shader == "-hs")
                    {
                        //need secondaryfile
                        OpenFileDialog ofd = new OpenFileDialog();
                        ofd.Title = "Select a Vertex shader needed to generate a Hull shader for Metal";
                        if (ofd.ShowDialog() == DialogResult.OK)
                        {
                            StreamReader sr = new StreamReader(ofd.FileName);
                            string vertexShaderData = sr.ReadToEnd();
                            string compositeData = vertexShaderData + richTextBox2.Text;

                            translatedMSL = PARSER(fileNames.ToArray(), compositeData, compositeData.Length, entryName, shader, "-msl", includeBuffers.ToArray(), includeCounter).Replace("\n", "\r\n");
                        }
                        else
                        {
                            outputWindow.richTextBoxOutput.Text = "Failed to load vertex shader for Metal's hull shader";
                            return;
                        }
                    }
                    else
                    {
                        translatedMSL = PARSER(fileNames.ToArray(), richTextBox2.Text, richTextBox2.TextLength, entryName, shader, "-msl", includeBuffers.ToArray(), includeCounter).Replace("\n", "\r\n");
                    }

                    outputWindow.buttonShowGLSL.PerformClick();
                }
                else if (languageString == "HLSL")
                {
                    translatedHLSL = PARSER(fileNames.ToArray(), richTextBox2.Text, richTextBox2.TextLength, entryName, shader, "-hlsl", includeBuffers.ToArray(), includeCounter).Replace("\n", "\r\n");
                    outputWindow.richTextBoxOutput.Text = richTextBox1.Text + "\r\n" + translatedHLSL;

                    if (outputWindow.richTextBoxOutput.Text == null)
                        outputWindow.richTextBoxOutput.Text = "Failed to translate";

                    //outputWindow.Update();
                    outputWindow.buttonShowHLSL.PerformClick();
                }
                else if (languageString == "GLSL")
                {
                    translatedGLSL = PARSER(fileNames.ToArray(), richTextBox2.Text, richTextBox2.TextLength, entryName, shader, "-glsl", includeBuffers.ToArray(), includeCounter).Replace("\n", "\r\n");
                    outputWindow.richTextBoxOutput.Text = richTextBox1.Text + "\r\n" + translatedGLSL;

                    if (outputWindow.richTextBoxOutput.Text == null)
                        outputWindow.richTextBoxOutput.Text = "Failed to translate";

                    //outputWindow.Update();
                    outputWindow.buttonShowGLSL.PerformClick();
                }
                else if (languageString == "MSL")
                {
                    if (shader == "-hs")
                    {
                        //need secondaryfile
                        OpenFileDialog ofd = new OpenFileDialog();
                        ofd.Title = "Select a Vertex shader needed to generate a Hull shader for Metal";
                        if (ofd.ShowDialog() == DialogResult.OK)
                        {
                            StreamReader sr = new StreamReader(ofd.FileName);
                            string vertexShaderData = sr.ReadToEnd();
                            string compositeData = vertexShaderData + richTextBox2.Text;

                            translatedMSL = PARSER(fileNames.ToArray(), compositeData, compositeData.Length, entryName, shader, "-msl", includeBuffers.ToArray(), includeCounter).Replace("\n", "\r\n");

                        }
                        else
                        {
                            outputWindow.richTextBoxOutput.Text = "Failed to load vertex shader for Metal's hull shader";
                            return;
                        }
                    }
                    else
                    {
                        translatedMSL = PARSER(fileNames.ToArray(), richTextBox2.Text, richTextBox2.TextLength, entryName, shader, "-msl", includeBuffers.ToArray(), includeCounter).Replace("\n", "\r\n");
                    }
                    
                    outputWindow.richTextBoxOutput.Text = richTextBox1.Text + "\r\n" + translatedMSL;

                    if (outputWindow.richTextBoxOutput.Text == null)
                        outputWindow.richTextBoxOutput.Text = "Failed to translate";

                    //outputWindow.Update();
                    outputWindow.buttonShowMSL.PerformClick();
                }


            }

            //Input Label Text
            if (fileNames != null)
            {


                char[] splitters = { '.', '\\' };
                string[] tokkens = fileNames[fileNames.Count()-1].Split(splitters);

                label5.Text = "Input from " + shaderString + " \"" + tokkens[tokkens.Length - 2] + "." + tokkens[tokkens.Length - 1] + "\"";
                label5.Update();
            }
        }

        private void richTextBox2_TextChanged(object sender, EventArgs e)
        {

        }

        private void Input_Load(object sender, EventArgs e)
        {

        }
    }
}
