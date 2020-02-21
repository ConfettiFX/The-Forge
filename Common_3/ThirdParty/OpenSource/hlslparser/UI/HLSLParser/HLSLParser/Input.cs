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
        [DllImport("ParserDll.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        public static extern string PARSER([MarshalAs(UnmanagedType.LPStr)] string _fileName, [MarshalAs(UnmanagedType.LPStr)] string entryName,
            [MarshalAs(UnmanagedType.LPStr)] string shader, [MarshalAs(UnmanagedType.LPStr)] string _language);

        Form1 parentForm;
        Output outputWindow;

        public string entryName = "main";

        public string fileName;
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

            richTextBox1.Text = "/*\r\n * Copyright (c) 2018-2020 The Forge Interactive Inc.\r\n * \r\n * This file is part of The-Forge\r\n * (see https://github.com/ConfettiFX/The-Forge). \r\n *\r\n * Licensed to the Apache Software Foundation (ASF) under one\r\n * or more contributor license agreements.  See the NOTICE file\r\n * distributed with this work for additional information\r\n * regarding copyright ownership.  The ASF licenses this file\r\n * to you under the Apache License, Version 2.0 (the\r\n * \"License\") you may not use this file except in compliance\r\n * with the License.  You may obtain a copy of the License at\r\n *\r\n *   http://www.apache.org/licenses/LICENSE-2.0\r\n *\r\n * Unless required by applicable law or agreed to in writing,\r\n * software distributed under the License is distributed on an\r\n * \"AS IS\" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY\r\n * KIND, either express or implied.  See the License for the\r\n * specific language governing permissions and limitations\r\n * under the License.\r\n*/\r\n";
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
                    translatedHLSL = PARSER(fileName, entryName, shader, "-hlsl").Replace("\n", "\r\n");
                    outputWindow.richTextBoxOutput.Text = richTextBox1.Text + "\r\n" + translatedHLSL;

                    if (outputWindow.richTextBoxOutput.Text == null)
                        outputWindow.richTextBoxOutput.Text = "Failed to translate";
                    
                    translatedGLSL = PARSER(fileName, entryName, shader, "-glsl").Replace("\n", "\r\n");

                    if (shader == "-hs")
                    {
						// TODO:????
                        //need secondaryfile
                        OpenFileDialog ofd = new OpenFileDialog();
                        ofd.Title = "Select a Vertex shader needed to generate a Hull shader for Metal";
                        if (ofd.ShowDialog() == DialogResult.OK)
                        {
                            StreamReader sr = new StreamReader(ofd.FileName);
                            string vertexShaderData = sr.ReadToEnd();
                            string compositeData = vertexShaderData + richTextBox2.Text;

                            translatedMSL = PARSER(fileName, entryName, shader, "-msl").Replace("\n", "\r\n");
                        }
                        else
                        {
                            outputWindow.richTextBoxOutput.Text = "Failed to load vertex shader for Metal's hull shader";
                            return;
                        }
                    }
                    else
                    {
                        translatedMSL = PARSER(fileName, entryName, shader, "-msl").Replace("\n", "\r\n");
                    }

                    outputWindow.buttonShowGLSL.PerformClick();
                }
                else if (languageString == "HLSL")
                {
                    translatedHLSL = PARSER(fileName, entryName, shader, "-hlsl").Replace("\n", "\r\n");
                    outputWindow.richTextBoxOutput.Text = richTextBox1.Text + "\r\n" + translatedHLSL;

                    if (outputWindow.richTextBoxOutput.Text == null)
                        outputWindow.richTextBoxOutput.Text = "Failed to translate";

                    //outputWindow.Update();
                    outputWindow.buttonShowHLSL.PerformClick();
                }
                else if (languageString == "GLSL")
                {
                    translatedGLSL = PARSER(fileName, entryName, shader, "-glsl").Replace("\n", "\r\n");
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

                            translatedMSL = PARSER(fileName, entryName, shader, "-msl").Replace("\n", "\r\n");

                        }
                        else
                        {
                            outputWindow.richTextBoxOutput.Text = "Failed to load vertex shader for Metal's hull shader";
                            return;
                        }
                    }
                    else
                    {
                        translatedMSL = PARSER(fileName, entryName, shader, "-msl").Replace("\n", "\r\n");
                    }
                    
                    outputWindow.richTextBoxOutput.Text = richTextBox1.Text + "\r\n" + translatedMSL;

                    if (outputWindow.richTextBoxOutput.Text == null)
                        outputWindow.richTextBoxOutput.Text = "Failed to translate";

                    //outputWindow.Update();
                    outputWindow.buttonShowMSL.PerformClick();
                }


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
