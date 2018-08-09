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

namespace HLSLParser
{
    public partial class Output : Form
    {
        SaveFileDialog sfd = new SaveFileDialog();

        Form1 parentForm;
        Input inputWindow;
        public Output(Form1 paramForm)
        {
            parentForm = paramForm;
            InitializeComponent();

            this.StartPosition = FormStartPosition.Manual;

            sfd.Filter = "vertex shader|*.vert|fragment shader|*.frag|hull shader|*.tesc|domain shader|*.tese|geometry shader|*.geom|compute shader|*.comp|all files|*.*";
            sfd.FilterIndex = 7;

            sfd.DefaultExt = "vert";
            sfd.AddExtension = true;
        }

        public void SetInputWindow(Input paramForm)
        {
            inputWindow = paramForm;
        }

        public string shader = "-vs";


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
                   sw.Write(richTextBoxOutput.Text);
                }
            }
        }       

        private void buttonShowHLSL_Click(object sender, EventArgs e)
        {
            
            richTextBoxOutput.Text = inputWindow.richTextBox1.Text + "\r\n" + inputWindow.translatedHLSL;
            richTextBoxOutput.Update();
           
            //Output Label Text
            //label3.Text = 
            Text = "Output to " + "HLSL" + " " + parentForm.shaderString;

            //label3.Update();

        }

        private void buttonShowGLSL_Click(object sender, EventArgs e)
        {
            richTextBoxOutput.Text = inputWindow.richTextBox1.Text + "\r\n" + inputWindow.translatedGLSL;
            richTextBoxOutput.Update();
           
            //Output Label Text
            //label3.Text = "Output to " + "GLSL" + " " + parentForm.shaderString;
            Text = "Output to " + "GLSL" + " " + parentForm.shaderString;
            //label3.Update();
        }

        private void buttonShowMSL_Click(object sender, EventArgs e)
        {
            richTextBoxOutput.Text = inputWindow.richTextBox1.Text + "\r\n" + inputWindow.translatedMSL;
            richTextBoxOutput.Update();
          
            //label3.Text = "Output to " + "MSL" + " " + parentForm.shaderString;
            Text = "Output to " + "MSL" + " " + parentForm.shaderString;
            //label3.Update();
        }

        private void richTextBoxOutput_TextChanged(object sender, EventArgs e)
        {

        }

        private void splitContainer1_Panel1_Paint(object sender, PaintEventArgs e)
        {

        }

        private void splitContainer1_Panel2_Paint(object sender, PaintEventArgs e)
        {

        }

        private void splitContainer1_SplitterMoved(object sender, SplitterEventArgs e)
        {

        }
    }
}
