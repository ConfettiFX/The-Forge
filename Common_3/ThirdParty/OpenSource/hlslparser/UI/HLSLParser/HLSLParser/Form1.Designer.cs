namespace HLSLParser
{
    partial class Form1
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.groupBox2 = new System.Windows.Forms.GroupBox();
            this.CSradioButton = new System.Windows.Forms.RadioButton();
            this.GSradioButton = new System.Windows.Forms.RadioButton();
            this.DSradioButton = new System.Windows.Forms.RadioButton();
            this.HSradioButton = new System.Windows.Forms.RadioButton();
            this.FSradioButton = new System.Windows.Forms.RadioButton();
            this.VSradioButton = new System.Windows.Forms.RadioButton();
            this.button1 = new System.Windows.Forms.Button();
            this.textBox1 = new System.Windows.Forms.TextBox();
            this.textBox2 = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.button2 = new System.Windows.Forms.Button();
            this.groupBox3 = new System.Windows.Forms.GroupBox();
            this.label2 = new System.Windows.Forms.Label();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.AllradioButton = new System.Windows.Forms.RadioButton();
            this.MSLradioButton = new System.Windows.Forms.RadioButton();
            this.GLSLradioButton = new System.Windows.Forms.RadioButton();
            this.HLSLradioButton = new System.Windows.Forms.RadioButton();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.label5 = new System.Windows.Forms.Label();
            this.richTextBox1 = new System.Windows.Forms.RichTextBox();
            this.richTextBox2 = new System.Windows.Forms.RichTextBox();
            this.richTextBoxOutputHLSL = new System.Windows.Forms.RichTextBox();
            this.button3 = new System.Windows.Forms.Button();
            this.panelHLSL = new System.Windows.Forms.Panel();
            this.richTextBoxOutputMSL = new System.Windows.Forms.RichTextBox();
            this.buttonShowHLSL = new System.Windows.Forms.Button();
            this.buttonShowGLSL = new System.Windows.Forms.Button();
            this.buttonShowMSL = new System.Windows.Forms.Button();
            this.richTextBoxOutputGLSL = new System.Windows.Forms.RichTextBox();
            this.panel1 = new System.Windows.Forms.Panel();
            this.panel2 = new System.Windows.Forms.Panel();
            this.groupBox2.SuspendLayout();
            this.groupBox3.SuspendLayout();
            this.groupBox1.SuspendLayout();
            this.panelHLSL.SuspendLayout();
            this.panel1.SuspendLayout();
            this.panel2.SuspendLayout();
            this.SuspendLayout();
            // 
            // groupBox2
            // 
            this.groupBox2.Controls.Add(this.CSradioButton);
            this.groupBox2.Controls.Add(this.GSradioButton);
            this.groupBox2.Controls.Add(this.DSradioButton);
            this.groupBox2.Controls.Add(this.HSradioButton);
            this.groupBox2.Controls.Add(this.FSradioButton);
            this.groupBox2.Controls.Add(this.VSradioButton);
            this.groupBox2.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.groupBox2.Location = new System.Drawing.Point(63, 37);
            this.groupBox2.Name = "groupBox2";
            this.groupBox2.Size = new System.Drawing.Size(240, 245);
            this.groupBox2.TabIndex = 3;
            this.groupBox2.TabStop = false;
            this.groupBox2.Text = "Shader";
            this.groupBox2.Enter += new System.EventHandler(this.groupBox2_Enter);
            // 
            // CSradioButton
            // 
            this.CSradioButton.AutoSize = true;
            this.CSradioButton.Location = new System.Drawing.Point(26, 195);
            this.CSradioButton.Name = "CSradioButton";
            this.CSradioButton.Size = new System.Drawing.Size(155, 24);
            this.CSradioButton.TabIndex = 5;
            this.CSradioButton.TabStop = true;
            this.CSradioButton.Text = "Compute Shader";
            this.CSradioButton.UseVisualStyleBackColor = true;
            this.CSradioButton.CheckedChanged += new System.EventHandler(this.CSradioButton_CheckedChanged);
            // 
            // GSradioButton
            // 
            this.GSradioButton.AutoSize = true;
            this.GSradioButton.Location = new System.Drawing.Point(26, 166);
            this.GSradioButton.Name = "GSradioButton";
            this.GSradioButton.Size = new System.Drawing.Size(160, 24);
            this.GSradioButton.TabIndex = 4;
            this.GSradioButton.TabStop = true;
            this.GSradioButton.Text = "Geometry Shader";
            this.GSradioButton.UseVisualStyleBackColor = true;
            this.GSradioButton.CheckedChanged += new System.EventHandler(this.GSradioButton_CheckedChanged);
            // 
            // DSradioButton
            // 
            this.DSradioButton.AutoSize = true;
            this.DSradioButton.Location = new System.Drawing.Point(26, 135);
            this.DSradioButton.Name = "DSradioButton";
            this.DSradioButton.Size = new System.Drawing.Size(145, 24);
            this.DSradioButton.TabIndex = 3;
            this.DSradioButton.TabStop = true;
            this.DSradioButton.Text = "Domain Shader";
            this.DSradioButton.UseVisualStyleBackColor = true;
            this.DSradioButton.CheckedChanged += new System.EventHandler(this.DSradioButton_CheckedChanged);
            // 
            // HSradioButton
            // 
            this.HSradioButton.AutoSize = true;
            this.HSradioButton.Location = new System.Drawing.Point(26, 106);
            this.HSradioButton.Name = "HSradioButton";
            this.HSradioButton.Size = new System.Drawing.Size(117, 24);
            this.HSradioButton.TabIndex = 2;
            this.HSradioButton.TabStop = true;
            this.HSradioButton.Text = "Hull Shader";
            this.HSradioButton.UseVisualStyleBackColor = true;
            this.HSradioButton.CheckedChanged += new System.EventHandler(this.HSradioButton_CheckedChanged);
            // 
            // FSradioButton
            // 
            this.FSradioButton.AutoSize = true;
            this.FSradioButton.Location = new System.Drawing.Point(26, 75);
            this.FSradioButton.Name = "FSradioButton";
            this.FSradioButton.Size = new System.Drawing.Size(159, 24);
            this.FSradioButton.TabIndex = 1;
            this.FSradioButton.TabStop = true;
            this.FSradioButton.Text = "Fragment Shader";
            this.FSradioButton.UseVisualStyleBackColor = true;
            this.FSradioButton.CheckedChanged += new System.EventHandler(this.FSradioButton_CheckedChanged);
            // 
            // VSradioButton
            // 
            this.VSradioButton.AutoSize = true;
            this.VSradioButton.Checked = true;
            this.VSradioButton.Location = new System.Drawing.Point(26, 46);
            this.VSradioButton.Name = "VSradioButton";
            this.VSradioButton.Size = new System.Drawing.Size(136, 24);
            this.VSradioButton.TabIndex = 0;
            this.VSradioButton.TabStop = true;
            this.VSradioButton.Text = "Vertex Shader";
            this.VSradioButton.UseVisualStyleBackColor = true;
            this.VSradioButton.CheckedChanged += new System.EventHandler(this.VSradioButton_CheckedChanged);
            // 
            // button1
            // 
            this.button1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.button1.Location = new System.Drawing.Point(708, 1023);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(57, 31);
            this.button1.TabIndex = 4;
            this.button1.Text = "...";
            this.button1.UseVisualStyleBackColor = false;
            this.button1.Click += new System.EventHandler(this.button1_Click);
            // 
            // textBox1
            // 
            this.textBox1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.textBox1.ForeColor = System.Drawing.SystemColors.InactiveBorder;
            this.textBox1.Location = new System.Drawing.Point(75, 1025);
            this.textBox1.Name = "textBox1";
            this.textBox1.Size = new System.Drawing.Size(644, 26);
            this.textBox1.TabIndex = 5;
            // 
            // textBox2
            // 
            this.textBox2.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.textBox2.ForeColor = System.Drawing.SystemColors.InactiveBorder;
            this.textBox2.Location = new System.Drawing.Point(78, 43);
            this.textBox2.Name = "textBox2";
            this.textBox2.Size = new System.Drawing.Size(141, 26);
            this.textBox2.TabIndex = 6;
            this.textBox2.Text = "main";
            this.textBox2.TextChanged += new System.EventHandler(this.textBox2_TextChanged);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(24, 46);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(50, 20);
            this.label1.TabIndex = 7;
            this.label1.Text = "Entry ";
            this.label1.Click += new System.EventHandler(this.label1_Click);
            // 
            // button2
            // 
            this.button2.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.button2.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.button2.Location = new System.Drawing.Point(868, 1119);
            this.button2.Name = "button2";
            this.button2.Size = new System.Drawing.Size(240, 72);
            this.button2.TabIndex = 8;
            this.button2.Text = "Translate!";
            this.button2.UseVisualStyleBackColor = false;
            this.button2.Click += new System.EventHandler(this.button2_Click);
            // 
            // groupBox3
            // 
            this.groupBox3.Controls.Add(this.label1);
            this.groupBox3.Controls.Add(this.textBox2);
            this.groupBox3.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.groupBox3.Location = new System.Drawing.Point(63, 520);
            this.groupBox3.Name = "groupBox3";
            this.groupBox3.Size = new System.Drawing.Size(240, 107);
            this.groupBox3.TabIndex = 6;
            this.groupBox3.TabStop = false;
            this.groupBox3.Text = "Inputs";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.label2.Location = new System.Drawing.Point(35, 1029);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(34, 20);
            this.label2.TabIndex = 8;
            this.label2.Text = "File";
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.AllradioButton);
            this.groupBox1.Controls.Add(this.MSLradioButton);
            this.groupBox1.Controls.Add(this.GLSLradioButton);
            this.groupBox1.Controls.Add(this.HLSLradioButton);
            this.groupBox1.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.groupBox1.Location = new System.Drawing.Point(63, 303);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(240, 179);
            this.groupBox1.TabIndex = 6;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Language";
            // 
            // AllradioButton
            // 
            this.AllradioButton.AutoSize = true;
            this.AllradioButton.Checked = true;
            this.AllradioButton.Location = new System.Drawing.Point(26, 136);
            this.AllradioButton.Name = "AllradioButton";
            this.AllradioButton.Size = new System.Drawing.Size(51, 24);
            this.AllradioButton.TabIndex = 3;
            this.AllradioButton.TabStop = true;
            this.AllradioButton.Text = "All";
            this.AllradioButton.UseVisualStyleBackColor = true;
            this.AllradioButton.CheckedChanged += new System.EventHandler(this.AllradioButton_CheckedChanged);
            // 
            // MSLradioButton
            // 
            this.MSLradioButton.AutoSize = true;
            this.MSLradioButton.Enabled = false;
            this.MSLradioButton.Location = new System.Drawing.Point(26, 106);
            this.MSLradioButton.Name = "MSLradioButton";
            this.MSLradioButton.Size = new System.Drawing.Size(67, 24);
            this.MSLradioButton.TabIndex = 2;
            this.MSLradioButton.Text = "MSL";
            this.MSLradioButton.UseVisualStyleBackColor = true;
            this.MSLradioButton.CheckedChanged += new System.EventHandler(this.MSLradioButton_CheckedChanged);
            // 
            // GLSLradioButton
            // 
            this.GLSLradioButton.AutoSize = true;
            this.GLSLradioButton.Location = new System.Drawing.Point(26, 75);
            this.GLSLradioButton.Name = "GLSLradioButton";
            this.GLSLradioButton.Size = new System.Drawing.Size(76, 24);
            this.GLSLradioButton.TabIndex = 1;
            this.GLSLradioButton.Text = "GLSL";
            this.GLSLradioButton.UseVisualStyleBackColor = true;
            this.GLSLradioButton.CheckedChanged += new System.EventHandler(this.GLSLradioButton_CheckedChanged);
            // 
            // HLSLradioButton
            // 
            this.HLSLradioButton.AutoSize = true;
            this.HLSLradioButton.Location = new System.Drawing.Point(26, 46);
            this.HLSLradioButton.Name = "HLSLradioButton";
            this.HLSLradioButton.Size = new System.Drawing.Size(75, 24);
            this.HLSLradioButton.TabIndex = 0;
            this.HLSLradioButton.Text = "HLSL";
            this.HLSLradioButton.UseVisualStyleBackColor = true;
            this.HLSLradioButton.CheckedChanged += new System.EventHandler(this.HLSLradioButton_CheckedChanged);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.label3.Location = new System.Drawing.Point(26, 23);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(58, 20);
            this.label3.TabIndex = 9;
            this.label3.Text = "Output";
            this.label3.Click += new System.EventHandler(this.label3_Click);
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.label4.Location = new System.Drawing.Point(29, 23);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(62, 20);
            this.label4.TabIndex = 11;
            this.label4.Text = "Header";
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.label5.Location = new System.Drawing.Point(29, 225);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(46, 20);
            this.label5.TabIndex = 12;
            this.label5.Text = "Input";
            this.label5.Click += new System.EventHandler(this.label5_Click);
            // 
            // richTextBox1
            // 
            this.richTextBox1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.richTextBox1.Font = new System.Drawing.Font("Consolas", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.richTextBox1.ForeColor = System.Drawing.SystemColors.InactiveBorder;
            this.richTextBox1.Location = new System.Drawing.Point(33, 60);
            this.richTextBox1.Name = "richTextBox1";
            this.richTextBox1.Size = new System.Drawing.Size(730, 148);
            this.richTextBox1.TabIndex = 13;
            this.richTextBox1.Text = "";
            this.richTextBox1.TextChanged += new System.EventHandler(this.richTextBox1_TextChanged);
            // 
            // richTextBox2
            // 
            this.richTextBox2.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.richTextBox2.Font = new System.Drawing.Font("Consolas", 10F);
            this.richTextBox2.ForeColor = System.Drawing.SystemColors.InactiveBorder;
            this.richTextBox2.Location = new System.Drawing.Point(33, 265);
            this.richTextBox2.Name = "richTextBox2";
            this.richTextBox2.Size = new System.Drawing.Size(730, 740);
            this.richTextBox2.TabIndex = 14;
            this.richTextBox2.Text = "";
            this.richTextBox2.TextChanged += new System.EventHandler(this.richTextBox2_TextChanged);
            // 
            // richTextBoxOutputHLSL
            // 
            this.richTextBoxOutputHLSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.richTextBoxOutputHLSL.Font = new System.Drawing.Font("Consolas", 10F);
            this.richTextBoxOutputHLSL.ForeColor = System.Drawing.SystemColors.InactiveBorder;
            this.richTextBoxOutputHLSL.Location = new System.Drawing.Point(30, 60);
            this.richTextBoxOutputHLSL.Name = "richTextBoxOutputHLSL";
            this.richTextBoxOutputHLSL.Size = new System.Drawing.Size(730, 1001);
            this.richTextBoxOutputHLSL.TabIndex = 15;
            this.richTextBoxOutputHLSL.Text = "";
            this.richTextBoxOutputHLSL.TextChanged += new System.EventHandler(this.richTextBoxOutputHLSL_TextChanged);
            // 
            // button3
            // 
            this.button3.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.button3.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.button3.Location = new System.Drawing.Point(510, 13);
            this.button3.Name = "button3";
            this.button3.Size = new System.Drawing.Size(250, 72);
            this.button3.TabIndex = 17;
            this.button3.Text = "Save";
            this.button3.UseVisualStyleBackColor = false;
            this.button3.Click += new System.EventHandler(this.button3_Click);
            // 
            // panelHLSL
            // 
            this.panelHLSL.Controls.Add(this.richTextBoxOutputMSL);
            this.panelHLSL.Controls.Add(this.richTextBoxOutputGLSL);
            this.panelHLSL.Controls.Add(this.richTextBoxOutputHLSL);
            this.panelHLSL.Controls.Add(this.label3);
            this.panelHLSL.Location = new System.Drawing.Point(1145, 12);
            this.panelHLSL.Name = "panelHLSL";
            this.panelHLSL.Size = new System.Drawing.Size(791, 1088);
            this.panelHLSL.TabIndex = 18;
            // 
            // richTextBoxOutputMSL
            // 
            this.richTextBoxOutputMSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.richTextBoxOutputMSL.Font = new System.Drawing.Font("Consolas", 10F);
            this.richTextBoxOutputMSL.ForeColor = System.Drawing.SystemColors.InactiveBorder;
            this.richTextBoxOutputMSL.Location = new System.Drawing.Point(30, 60);
            this.richTextBoxOutputMSL.Name = "richTextBoxOutputMSL";
            this.richTextBoxOutputMSL.Size = new System.Drawing.Size(730, 1001);
            this.richTextBoxOutputMSL.TabIndex = 15;
            this.richTextBoxOutputMSL.Text = "";
            // 
            // buttonShowHLSL
            // 
            this.buttonShowHLSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.buttonShowHLSL.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.buttonShowHLSL.Location = new System.Drawing.Point(30, 13);
            this.buttonShowHLSL.Name = "buttonShowHLSL";
            this.buttonShowHLSL.Size = new System.Drawing.Size(154, 72);
            this.buttonShowHLSL.TabIndex = 21;
            this.buttonShowHLSL.Text = "ShowHLSL";
            this.buttonShowHLSL.UseVisualStyleBackColor = false;
            this.buttonShowHLSL.Click += new System.EventHandler(this.buttonShowHLSL_Click);
            // 
            // buttonShowGLSL
            // 
            this.buttonShowGLSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.buttonShowGLSL.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.buttonShowGLSL.Location = new System.Drawing.Point(190, 13);
            this.buttonShowGLSL.Name = "buttonShowGLSL";
            this.buttonShowGLSL.Size = new System.Drawing.Size(154, 72);
            this.buttonShowGLSL.TabIndex = 22;
            this.buttonShowGLSL.Text = "ShowGLSL";
            this.buttonShowGLSL.UseVisualStyleBackColor = false;
            this.buttonShowGLSL.Click += new System.EventHandler(this.buttonShowGLSL_Click);
            // 
            // buttonShowMSL
            // 
            this.buttonShowMSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.buttonShowMSL.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(192)))), ((int)(((byte)(128)))));
            this.buttonShowMSL.Location = new System.Drawing.Point(350, 13);
            this.buttonShowMSL.Name = "buttonShowMSL";
            this.buttonShowMSL.Size = new System.Drawing.Size(154, 72);
            this.buttonShowMSL.TabIndex = 23;
            this.buttonShowMSL.Text = "ShowMSL";
            this.buttonShowMSL.UseVisualStyleBackColor = false;
            this.buttonShowMSL.Click += new System.EventHandler(this.buttonShowMSL_Click);
            // 
            // richTextBoxOutputGLSL
            // 
            this.richTextBoxOutputGLSL.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(28)))), ((int)(((byte)(28)))), ((int)(((byte)(28)))));
            this.richTextBoxOutputGLSL.Font = new System.Drawing.Font("Consolas", 10F);
            this.richTextBoxOutputGLSL.ForeColor = System.Drawing.SystemColors.InactiveBorder;
            this.richTextBoxOutputGLSL.Location = new System.Drawing.Point(30, 60);
            this.richTextBoxOutputGLSL.Name = "richTextBoxOutputGLSL";
            this.richTextBoxOutputGLSL.Size = new System.Drawing.Size(730, 1001);
            this.richTextBoxOutputGLSL.TabIndex = 15;
            this.richTextBoxOutputGLSL.Text = "";
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.buttonShowHLSL);
            this.panel1.Controls.Add(this.buttonShowMSL);
            this.panel1.Controls.Add(this.button3);
            this.panel1.Controls.Add(this.buttonShowGLSL);
            this.panel1.Location = new System.Drawing.Point(1145, 1106);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(791, 100);
            this.panel1.TabIndex = 24;
            // 
            // panel2
            // 
            this.panel2.Controls.Add(this.label2);
            this.panel2.Controls.Add(this.label4);
            this.panel2.Controls.Add(this.button1);
            this.panel2.Controls.Add(this.label5);
            this.panel2.Controls.Add(this.richTextBox1);
            this.panel2.Controls.Add(this.textBox1);
            this.panel2.Controls.Add(this.richTextBox2);
            this.panel2.Location = new System.Drawing.Point(345, 12);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(791, 1088);
            this.panel2.TabIndex = 25;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(9F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoSize = true;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(45)))), ((int)(((byte)(45)))), ((int)(((byte)(48)))));
            this.ClientSize = new System.Drawing.Size(1978, 1224);
            this.Controls.Add(this.groupBox2);
            this.Controls.Add(this.groupBox1);
            this.Controls.Add(this.panel2);
            this.Controls.Add(this.groupBox3);
            this.Controls.Add(this.panel1);
            this.Controls.Add(this.panelHLSL);
            this.Controls.Add(this.button2);
            this.ForeColor = System.Drawing.SystemColors.ButtonHighlight;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "Form1";
            this.Text = "HLSLparser - Alpha";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.groupBox2.ResumeLayout(false);
            this.groupBox2.PerformLayout();
            this.groupBox3.ResumeLayout(false);
            this.groupBox3.PerformLayout();
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.panelHLSL.ResumeLayout(false);
            this.panelHLSL.PerformLayout();
            this.panel1.ResumeLayout(false);
            this.panel2.ResumeLayout(false);
            this.panel2.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion
        private System.Windows.Forms.GroupBox groupBox2;
        private System.Windows.Forms.RadioButton CSradioButton;
        private System.Windows.Forms.RadioButton GSradioButton;
        private System.Windows.Forms.RadioButton DSradioButton;
        private System.Windows.Forms.RadioButton HSradioButton;
        private System.Windows.Forms.RadioButton FSradioButton;
        private System.Windows.Forms.RadioButton VSradioButton;
        private System.Windows.Forms.Button button1;
        private System.Windows.Forms.TextBox textBox1;
        private System.Windows.Forms.TextBox textBox2;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button button2;
        private System.Windows.Forms.GroupBox groupBox3;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.RadioButton MSLradioButton;
        private System.Windows.Forms.RadioButton GLSLradioButton;
        private System.Windows.Forms.RadioButton HLSLradioButton;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.RichTextBox richTextBox1;
        private System.Windows.Forms.RichTextBox richTextBox2;
        private System.Windows.Forms.RichTextBox richTextBoxOutputHLSL;
        private System.Windows.Forms.RadioButton AllradioButton;
        private System.Windows.Forms.Button button3;
        private System.Windows.Forms.Panel panelHLSL;
        private System.Windows.Forms.RichTextBox richTextBoxOutputMSL;
        private System.Windows.Forms.Button buttonShowHLSL;
        private System.Windows.Forms.Button buttonShowGLSL;
        private System.Windows.Forms.Button buttonShowMSL;
        private System.Windows.Forms.RichTextBox richTextBoxOutputGLSL;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Panel panel2;
    }
}

