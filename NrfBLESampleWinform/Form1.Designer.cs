namespace NrfBLESampleWinform
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
            this.textBoxLog = new System.Windows.Forms.TextBox();
            this.textBoxSerialPort = new System.Windows.Forms.TextBox();
            this.textBoxBaudRate = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.buttonDongleInit = new System.Windows.Forms.Button();
            this.textBoxRssi = new System.Windows.Forms.TextBox();
            this.textBoxAddress = new System.Windows.Forms.TextBox();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.buttonScanStart = new System.Windows.Forms.Button();
            this.comboBoxReportChar = new System.Windows.Forms.ComboBox();
            this.label5 = new System.Windows.Forms.Label();
            this.textBoxWriteData = new System.Windows.Forms.TextBox();
            this.textBoxReadData = new System.Windows.Forms.TextBox();
            this.buttonDisconnect = new System.Windows.Forms.Button();
            this.buttonDongleReset = new System.Windows.Forms.Button();
            this.buttonDongleClose = new System.Windows.Forms.Button();
            this.buttonConnect = new System.Windows.Forms.Button();
            this.buttonWrite = new System.Windows.Forms.Button();
            this.buttonRead = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // textBoxLog
            // 
            this.textBoxLog.Location = new System.Drawing.Point(12, 12);
            this.textBoxLog.Multiline = true;
            this.textBoxLog.Name = "textBoxLog";
            this.textBoxLog.ReadOnly = true;
            this.textBoxLog.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.textBoxLog.Size = new System.Drawing.Size(550, 425);
            this.textBoxLog.TabIndex = 0;
            // 
            // textBoxSerialPort
            // 
            this.textBoxSerialPort.Location = new System.Drawing.Point(652, 12);
            this.textBoxSerialPort.Name = "textBoxSerialPort";
            this.textBoxSerialPort.Size = new System.Drawing.Size(120, 24);
            this.textBoxSerialPort.TabIndex = 1;
            this.textBoxSerialPort.Text = "COM3";
            // 
            // textBoxBaudRate
            // 
            this.textBoxBaudRate.Location = new System.Drawing.Point(652, 42);
            this.textBoxBaudRate.Name = "textBoxBaudRate";
            this.textBoxBaudRate.Size = new System.Drawing.Size(120, 24);
            this.textBoxBaudRate.TabIndex = 2;
            this.textBoxBaudRate.Text = "10000";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(569, 15);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(66, 17);
            this.label1.TabIndex = 3;
            this.label1.Text = "Serial port";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(569, 45);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(64, 17);
            this.label2.TabIndex = 4;
            this.label2.Text = "Baud rate";
            // 
            // buttonDongleInit
            // 
            this.buttonDongleInit.Location = new System.Drawing.Point(652, 72);
            this.buttonDongleInit.Name = "buttonDongleInit";
            this.buttonDongleInit.Size = new System.Drawing.Size(120, 25);
            this.buttonDongleInit.TabIndex = 5;
            this.buttonDongleInit.Text = "Initialize";
            this.buttonDongleInit.UseVisualStyleBackColor = true;
            // 
            // textBoxRssi
            // 
            this.textBoxRssi.Location = new System.Drawing.Point(652, 133);
            this.textBoxRssi.Name = "textBoxRssi";
            this.textBoxRssi.Size = new System.Drawing.Size(120, 24);
            this.textBoxRssi.TabIndex = 7;
            this.textBoxRssi.Text = "-50";
            // 
            // textBoxAddress
            // 
            this.textBoxAddress.Location = new System.Drawing.Point(652, 103);
            this.textBoxAddress.Name = "textBoxAddress";
            this.textBoxAddress.Size = new System.Drawing.Size(120, 24);
            this.textBoxAddress.TabIndex = 6;
            this.textBoxAddress.Text = "112233445566";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(569, 106);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(52, 17);
            this.label3.TabIndex = 9;
            this.label3.Text = "Address";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(569, 136);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(29, 17);
            this.label4.TabIndex = 8;
            this.label4.Text = "Rssi";
            // 
            // buttonScanStart
            // 
            this.buttonScanStart.Location = new System.Drawing.Point(652, 163);
            this.buttonScanStart.Name = "buttonScanStart";
            this.buttonScanStart.Size = new System.Drawing.Size(120, 25);
            this.buttonScanStart.TabIndex = 10;
            this.buttonScanStart.Text = "Scan";
            this.buttonScanStart.UseVisualStyleBackColor = true;
            // 
            // comboBoxReportChar
            // 
            this.comboBoxReportChar.FormattingEnabled = true;
            this.comboBoxReportChar.Location = new System.Drawing.Point(622, 237);
            this.comboBoxReportChar.Name = "comboBoxReportChar";
            this.comboBoxReportChar.Size = new System.Drawing.Size(150, 25);
            this.comboBoxReportChar.TabIndex = 11;
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(569, 240);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(47, 17);
            this.label5.TabIndex = 12;
            this.label5.Text = "Report";
            // 
            // textBoxWriteData
            // 
            this.textBoxWriteData.Location = new System.Drawing.Point(622, 268);
            this.textBoxWriteData.Name = "textBoxWriteData";
            this.textBoxWriteData.Size = new System.Drawing.Size(150, 24);
            this.textBoxWriteData.TabIndex = 13;
            this.textBoxWriteData.Text = "00 00 00 00";
            // 
            // textBoxReadData
            // 
            this.textBoxReadData.Location = new System.Drawing.Point(622, 298);
            this.textBoxReadData.Name = "textBoxReadData";
            this.textBoxReadData.ReadOnly = true;
            this.textBoxReadData.Size = new System.Drawing.Size(150, 24);
            this.textBoxReadData.TabIndex = 15;
            // 
            // buttonDisconnect
            // 
            this.buttonDisconnect.Location = new System.Drawing.Point(652, 344);
            this.buttonDisconnect.Name = "buttonDisconnect";
            this.buttonDisconnect.Size = new System.Drawing.Size(120, 25);
            this.buttonDisconnect.TabIndex = 17;
            this.buttonDisconnect.Text = "Disconnect";
            this.buttonDisconnect.UseVisualStyleBackColor = true;
            // 
            // buttonDongleReset
            // 
            this.buttonDongleReset.Location = new System.Drawing.Point(652, 375);
            this.buttonDongleReset.Name = "buttonDongleReset";
            this.buttonDongleReset.Size = new System.Drawing.Size(120, 25);
            this.buttonDongleReset.TabIndex = 18;
            this.buttonDongleReset.Text = "Dongle Reset";
            this.buttonDongleReset.UseVisualStyleBackColor = true;
            // 
            // buttonDongleClose
            // 
            this.buttonDongleClose.Location = new System.Drawing.Point(652, 406);
            this.buttonDongleClose.Name = "buttonDongleClose";
            this.buttonDongleClose.Size = new System.Drawing.Size(120, 25);
            this.buttonDongleClose.TabIndex = 19;
            this.buttonDongleClose.Text = "Dongle Close";
            this.buttonDongleClose.UseVisualStyleBackColor = true;
            // 
            // buttonConnect
            // 
            this.buttonConnect.Location = new System.Drawing.Point(652, 194);
            this.buttonConnect.Name = "buttonConnect";
            this.buttonConnect.Size = new System.Drawing.Size(120, 25);
            this.buttonConnect.TabIndex = 20;
            this.buttonConnect.Text = "Connect";
            this.buttonConnect.UseVisualStyleBackColor = true;
            // 
            // buttonWrite
            // 
            this.buttonWrite.Location = new System.Drawing.Point(568, 268);
            this.buttonWrite.Name = "buttonWrite";
            this.buttonWrite.Size = new System.Drawing.Size(48, 25);
            this.buttonWrite.TabIndex = 21;
            this.buttonWrite.Text = "Write";
            this.buttonWrite.UseVisualStyleBackColor = true;
            // 
            // buttonRead
            // 
            this.buttonRead.Location = new System.Drawing.Point(568, 297);
            this.buttonRead.Name = "buttonRead";
            this.buttonRead.Size = new System.Drawing.Size(48, 25);
            this.buttonRead.TabIndex = 22;
            this.buttonRead.Text = "Read";
            this.buttonRead.UseVisualStyleBackColor = true;
            // 
            // Form1
            // 
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.None;
            this.ClientSize = new System.Drawing.Size(784, 461);
            this.Controls.Add(this.buttonRead);
            this.Controls.Add(this.buttonWrite);
            this.Controls.Add(this.buttonConnect);
            this.Controls.Add(this.buttonDongleClose);
            this.Controls.Add(this.buttonDongleReset);
            this.Controls.Add(this.buttonDisconnect);
            this.Controls.Add(this.textBoxReadData);
            this.Controls.Add(this.textBoxWriteData);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.comboBoxReportChar);
            this.Controls.Add(this.buttonScanStart);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.textBoxRssi);
            this.Controls.Add(this.textBoxAddress);
            this.Controls.Add(this.buttonDongleInit);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.textBoxBaudRate);
            this.Controls.Add(this.textBoxSerialPort);
            this.Controls.Add(this.textBoxLog);
            this.Font = new System.Drawing.Font("Calibri", 10.2F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "Form1";
            this.Text = "Form1";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TextBox textBoxLog;
        private System.Windows.Forms.TextBox textBoxSerialPort;
        private System.Windows.Forms.TextBox textBoxBaudRate;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Button buttonDongleInit;
        private System.Windows.Forms.TextBox textBoxRssi;
        private System.Windows.Forms.TextBox textBoxAddress;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Button buttonScanStart;
        private System.Windows.Forms.ComboBox comboBoxReportChar;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.TextBox textBoxWriteData;
        private System.Windows.Forms.TextBox textBoxReadData;
        private System.Windows.Forms.Button buttonDisconnect;
        private System.Windows.Forms.Button buttonDongleReset;
        private System.Windows.Forms.Button buttonDongleClose;
        private System.Windows.Forms.Button buttonConnect;
        private System.Windows.Forms.Button buttonWrite;
        private System.Windows.Forms.Button buttonRead;
    }
}

