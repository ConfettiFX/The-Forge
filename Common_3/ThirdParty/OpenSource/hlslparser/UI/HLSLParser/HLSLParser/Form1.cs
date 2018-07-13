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
        [DllImport("hlslparser.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        public static extern string PARSER([MarshalAs(UnmanagedType.LPStr)]string fileName,  [MarshalAs(UnmanagedType.LPStr)]string buffer, int bufferSize, [MarshalAs(UnmanagedType.LPStr)]string entryName, [MarshalAs(UnmanagedType.LPStr)]string shader, [MarshalAs(UnmanagedType.LPStr)]string _language);

        OpenFileDialog ofd = new OpenFileDialog();
        SaveFileDialog sfd = new SaveFileDialog();

        public Form1()
        {
            InitializeComponent();

            richTextBox1.Text = "/*\r\n * Copyright (c) 2018 Confetti Interactive Inc.\r\n * \r\n * This file is part of The-Forge\r\n * (see https://github.com/ConfettiFX/The-Forge). \r\n *\r\n * Licensed to the Apache Software Foundation (ASF) under one\r\n * or more contributor license agreements.  See the NOTICE file\r\n * distributed with this work for additional information\r\n * regarding copyright ownership.  The ASF licenses this file\r\n * to you under the Apache License, Version 2.0 (the\r\n * \"License\") you may not use this file except in compliance\r\n * with the License.  You may obtain a copy of the License at\r\n *\r\n *   http://www.apache.org/licenses/LICENSE-2.0\r\n *\r\n * Unless required by applicable law or agreed to in writing,\r\n * software distributed under the License is distributed on an\r\n * \"AS IS\" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY\r\n * KIND, either express or implied.  See the License for the\r\n * specific language governing permissions and limitations\r\n * under the License.\r\n*/\r\n";
            richTextBox1.Update();

            /*
            panelHLSL.Show();
            panelGLSL.Hide();
            panelMSL.Hide();
            */

            /*
            float currentSize = textBox4.Font.SizeInPoints;
            textBox4.Font = new Font(textBox4.Font.Name, 30.0f, textBox4.Font.Style);
            textBox4.Update();
            */
            
            sfd.Filter = "vertex shader|*.vert|fragment shader|*.frag|hull shader|*.tesc|domain shader|*.tese|geometry shader|*.geom|compute shader|*.comp|all files|*.*";
            sfd.FilterIndex = 7;

            sfd.DefaultExt = "vert";
            sfd.AddExtension = true;
        }


        bool bTranslateAll = true;
       

        string entryName = "main";
        string fileName;
        string shader = "-vs";
        string language = "-glsl";
        string shaderString = "Vertex shader";
        string languageString = "GLSL";

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        private void radioButton1_CheckedChanged(object sender, EventArgs e)
        {

        }

        //fsButton
        private void FSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-fs";
            shaderString = "Fragment shader";
        }

        //vsButton
        private void VSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-vs";
            shaderString = "Vertex shader";
        }

        //hsButton
        private void HSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-hs";
            shaderString = "Hull shader";
        }

        //dsButton
        private void DSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-ds";
            shaderString = "Domain shader";
        }

        //gsButton
        private void GSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-gs";
            shaderString = "Geometry shader";
        }

        //csButton
        private void CSradioButton_CheckedChanged(object sender, EventArgs e)
        {
            shader = "-cs";
            shaderString = "Compute shader";
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
        }

        private void GLSLradioButton_CheckedChanged(object sender, EventArgs e)
        {
            language = "-glsl";
            languageString = "GLSL";
            bTranslateAll = false;
        }

        private void MSLradioButton_CheckedChanged(object sender, EventArgs e)
        {
            language = "-msl";
            languageString = "MSL";
            bTranslateAll = false;
        }

        private void AllradioButton_CheckedChanged(object sender, EventArgs e)
        {
            bTranslateAll = AllradioButton.Checked;
        }

        //parser
        private void button2_Click(object sender, EventArgs e)
        {
            if(richTextBox2.Text != "")
            {
                if(bTranslateAll)
                {
                    richTextBoxOutputHLSL.Text = richTextBox1.Text + "\r\n" + PARSER(fileName, richTextBox2.Text, richTextBox2.TextLength, entryName, shader, "-hlsl").Replace("\n", "\r\n");

                    if (richTextBoxOutputHLSL.Text == null)
                        richTextBoxOutputHLSL.Text = "Failed to translate";

                    richTextBoxOutputHLSL.Update();

                    richTextBoxOutputGLSL.Text = richTextBox1.Text + "\r\n" + PARSER(fileName, richTextBox2.Text, richTextBox2.TextLength, entryName, shader, "-glsl").Replace("\n", "\r\n");

                    if (richTextBoxOutputGLSL.Text == null)
                        richTextBoxOutputGLSL.Text = "Failed to translate";

                    richTextBoxOutputGLSL.Update();

                    /*
                    richTextBoxOutputMSL.Text = richTextBox1.Text + "\r\n" + PARSER(fileName, richTextBox2.Text, richTextBox2.TextLength, entryName, shader, ).Replace("\n", "\r\n");

                    if (richTextBoxOutputMSL.Text == null)
                        richTextBoxOutputMSL.Text = "Failed to translate";

                    richTextBoxOutputMSL.Update();
                    */

                    buttonShowHLSL.PerformClick();                   
                }
                else if(languageString == "HLSL")
                {
                    richTextBoxOutputHLSL.Text = richTextBox1.Text + "\r\n" + PARSER(fileName, richTextBox2.Text, richTextBox2.TextLength, entryName, shader, language).Replace("\n", "\r\n");

                    if (richTextBoxOutputHLSL.Text == null)
                        richTextBoxOutputHLSL.Text = "Failed to translate";

                    richTextBoxOutputHLSL.Update();
                }
                else if (languageString == "GLSL")
                {
                    richTextBoxOutputGLSL.Text = richTextBox1.Text + "\r\n" + PARSER(fileName, richTextBox2.Text, richTextBox2.TextLength, entryName, shader, language).Replace("\n", "\r\n");

                    if (richTextBoxOutputGLSL.Text == null)
                        richTextBoxOutputGLSL.Text = "Failed to translate";

                    richTextBoxOutputGLSL.Update();
                }
                else if (languageString == "MSL")
                {
                    richTextBoxOutputMSL.Text = richTextBox1.Text + "\r\n" + PARSER(fileName, richTextBox2.Text, richTextBox2.TextLength, entryName, shader, language).Replace("\n", "\r\n");

                    if (richTextBoxOutputMSL.Text == null)
                        richTextBoxOutputMSL.Text = "Failed to translate";

                    richTextBoxOutputMSL.Update();
                }
            }

            //Input Label Text
            if(fileName != null)
            {
                char[] splitters = { '.', '\\' };
                string[] tokkens = fileName.Split(splitters);

                label5.Text = "Input from " + shaderString + " \"" + tokkens[tokkens.Length - 2] + "." + tokkens[tokkens.Length - 1] + "\"";
                label5.Update();
            }
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

        private void button1_Click(object sender, EventArgs e)
        {
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                textBox1.Text = fileName = ofd.FileName;
                StreamReader sr = new StreamReader(textBox1.Text);
                string inputData = sr.ReadToEnd();// ReadLine();

                richTextBox2.Text = inputData;

                char[] splitters = { '.', '\\' };
                string[] tokkens = fileName.Split(splitters);

                switch(tokkens[tokkens.Length - 1])
                {
                    case "vert": VSradioButton.Checked = true; break;
                    case "frag": FSradioButton.Checked = true; break;
                    case "tesc": HSradioButton.Checked = true; break;
                    case "tese": DSradioButton.Checked = true; break;
                    case "geom": GSradioButton.Checked = true; break;
                    case "comp": CSradioButton.Checked = true; break;
                    default: break;
                }

                label5.Text = "Input from " + shaderString + " \"" + tokkens[tokkens.Length - 2] + "." + tokkens[tokkens.Length - 1] + "\"";
                label5.Update();
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
                using (Stream s = File.Open(sfd.FileName, FileMode.CreateNew))
                using (StreamWriter sw = new StreamWriter(s))
                {
                    if (richTextBoxOutputHLSL.Visible)
                        sw.Write(richTextBoxOutputHLSL.Text);
                    else if (richTextBoxOutputGLSL.Visible)
                        sw.Write(richTextBoxOutputGLSL.Text);
                    else if (richTextBoxOutputMSL.Visible)
                        sw.Write(richTextBoxOutputMSL.Text);
                }
            }
        }

        private void textBox2_TextChanged(object sender, EventArgs e)
        {
            entryName = textBox2.Text;
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

        private void buttonShowHLSL_Click(object sender, EventArgs e)
        {

            richTextBoxOutputHLSL.Show();
            richTextBoxOutputGLSL.Hide();
            richTextBoxOutputMSL.Hide();

            //Output Label Text
            label3.Text = "Output to " + "HLSL" + " " + shaderString;
            label3.Update();

        }

        private void buttonShowGLSL_Click(object sender, EventArgs e)
        {
            richTextBoxOutputHLSL.Hide();
            richTextBoxOutputGLSL.Show();
            richTextBoxOutputMSL.Hide();

            //Output Label Text
            label3.Text = "Output to " + "GLSL" + " " + shaderString;
            label3.Update();
        }

        private void buttonShowMSL_Click(object sender, EventArgs e)
        {
            richTextBoxOutputHLSL.Hide();
            richTextBoxOutputGLSL.Hide();
            richTextBoxOutputMSL.Show();

            //Output Label Text
            label3.Text = "Output to " + "MSL" + " " + shaderString;
            label3.Update();
        }
    }
}
