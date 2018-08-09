namespace HLSLParser
{
    partial class Output
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Output));
            this.buttonShowHLSL = new System.Windows.Forms.Button();
            this.button3 = new System.Windows.Forms.Button();
            this.buttonShowGLSL = new System.Windows.Forms.Button();
            this.buttonShowMSL = new System.Windows.Forms.Button();
            this.richTextBoxOutput = new System.Windows.Forms.RichTextBox();
            this.panel1 = new System.Windows.Forms.Panel();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.panel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.SuspendLayout();
            // 
            // buttonShowHLSL
            // 
            this.buttonShowHLSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.buttonShowHLSL.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.buttonShowHLSL.Location = new System.Drawing.Point(6, 0);
            this.buttonShowHLSL.Name = "buttonShowHLSL";
            this.buttonShowHLSL.Size = new System.Drawing.Size(165, 75);
            this.buttonShowHLSL.TabIndex = 21;
            this.buttonShowHLSL.Text = "ShowHLSL";
            this.buttonShowHLSL.UseVisualStyleBackColor = false;
            this.buttonShowHLSL.Click += new System.EventHandler(this.buttonShowHLSL_Click);
            // 
            // button3
            // 
            this.button3.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.button3.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.button3.Location = new System.Drawing.Point(572, 0);
            this.button3.Name = "button3";
            this.button3.Size = new System.Drawing.Size(165, 75);
            this.button3.TabIndex = 17;
            this.button3.Text = "Save";
            this.button3.UseVisualStyleBackColor = false;
            this.button3.Click += new System.EventHandler(this.button3_Click);
            // 
            // buttonShowGLSL
            // 
            this.buttonShowGLSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.buttonShowGLSL.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.buttonShowGLSL.Location = new System.Drawing.Point(177, 0);
            this.buttonShowGLSL.Name = "buttonShowGLSL";
            this.buttonShowGLSL.Size = new System.Drawing.Size(165, 75);
            this.buttonShowGLSL.TabIndex = 22;
            this.buttonShowGLSL.Text = "ShowGLSL";
            this.buttonShowGLSL.UseVisualStyleBackColor = false;
            this.buttonShowGLSL.Click += new System.EventHandler(this.buttonShowGLSL_Click);
            // 
            // buttonShowMSL
            // 
            this.buttonShowMSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.buttonShowMSL.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.buttonShowMSL.Location = new System.Drawing.Point(348, 0);
            this.buttonShowMSL.Name = "buttonShowMSL";
            this.buttonShowMSL.Size = new System.Drawing.Size(165, 75);
            this.buttonShowMSL.TabIndex = 23;
            this.buttonShowMSL.Text = "ShowMSL";
            this.buttonShowMSL.UseVisualStyleBackColor = false;
            this.buttonShowMSL.Click += new System.EventHandler(this.buttonShowMSL_Click);
            // 
            // richTextBoxOutput
            // 
            this.richTextBoxOutput.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.richTextBoxOutput.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.richTextBoxOutput.Font = new System.Drawing.Font("Consolas", 10F);
            this.richTextBoxOutput.ForeColor = System.Drawing.SystemColors.InactiveBorder;
            this.richTextBoxOutput.Location = new System.Drawing.Point(3, 3);
            this.richTextBoxOutput.Name = "richTextBoxOutput";
            this.richTextBoxOutput.Size = new System.Drawing.Size(738, 1237);
            this.richTextBoxOutput.TabIndex = 15;
            this.richTextBoxOutput.Text = "";
            this.richTextBoxOutput.WordWrap = false;
            this.richTextBoxOutput.TextChanged += new System.EventHandler(this.richTextBoxOutput_TextChanged);
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.buttonShowHLSL);
            this.panel1.Controls.Add(this.button3);
            this.panel1.Controls.Add(this.buttonShowGLSL);
            this.panel1.Controls.Add(this.buttonShowMSL);
            this.panel1.Location = new System.Drawing.Point(3, 3);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(747, 83);
            this.panel1.TabIndex = 25;
            // 
            // splitContainer1
            // 
            this.splitContainer1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            this.splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.richTextBoxOutput);
            this.splitContainer1.Panel1.Paint += new System.Windows.Forms.PaintEventHandler(this.splitContainer1_Panel1_Paint);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.panel1);
            this.splitContainer1.Panel2.Paint += new System.Windows.Forms.PaintEventHandler(this.splitContainer1_Panel2_Paint);
            this.splitContainer1.Size = new System.Drawing.Size(759, 1319);
            this.splitContainer1.SplitterDistance = 1237;
            this.splitContainer1.TabIndex = 26;
            this.splitContainer1.SplitterMoved += new System.Windows.Forms.SplitterEventHandler(this.splitContainer1_SplitterMoved);
            // 
            // Output
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(9F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(45)))), ((int)(((byte)(45)))), ((int)(((byte)(48)))));
            this.ClientSize = new System.Drawing.Size(753, 1331);
            this.Controls.Add(this.splitContainer1);
            this.ForeColor = System.Drawing.SystemColors.ButtonHighlight;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "Output";
            this.Text = "Output";
            this.panel1.ResumeLayout(false);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion
        public System.Windows.Forms.Button buttonShowHLSL;
        public System.Windows.Forms.Button buttonShowGLSL;
        public System.Windows.Forms.Button buttonShowMSL;
        private System.Windows.Forms.Button button3;
        public System.Windows.Forms.RichTextBox richTextBoxOutput;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.SplitContainer splitContainer1;
    }
}