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
            this.buttonConnect = new System.Windows.Forms.Button();
            this.buttonWrite = new System.Windows.Forms.Button();
            this.buttonRead = new System.Windows.Forms.Button();
            this.label6 = new System.Windows.Forms.Label();
            this.comboBoxDiscovered = new System.Windows.Forms.ComboBox();
            this.buttonScanStop = new System.Windows.Forms.Button();
            this.checkBoxLesc = new System.Windows.Forms.CheckBox();
            this.checkBoxOob = new System.Windows.Forms.CheckBox();
            this.checkBoxMitm = new System.Windows.Forms.CheckBox();
            this.checkBoxKeypress = new System.Windows.Forms.CheckBox();
            this.checkBoxKdistId = new System.Windows.Forms.CheckBox();
            this.checkBoxKdistEnc = new System.Windows.Forms.CheckBox();
            this.label7 = new System.Windows.Forms.Label();
            this.label8 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // textBoxLog
            // 
            this.textBoxLog.Location = new System.Drawing.Point(12, 43);
            this.textBoxLog.Multiline = true;
            this.textBoxLog.Name = "textBoxLog";
            this.textBoxLog.ReadOnly = true;
            this.textBoxLog.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.textBoxLog.Size = new System.Drawing.Size(550, 394);
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
            this.textBoxBaudRate.Text = "1000000";
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
            this.textBoxAddress.Text = "0";
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
            this.buttonScanStart.Text = "Scan start";
            this.buttonScanStart.UseVisualStyleBackColor = true;
            // 
            // comboBoxReportChar
            // 
            this.comboBoxReportChar.FormattingEnabled = true;
            this.comboBoxReportChar.Location = new System.Drawing.Point(622, 298);
            this.comboBoxReportChar.Name = "comboBoxReportChar";
            this.comboBoxReportChar.Size = new System.Drawing.Size(150, 25);
            this.comboBoxReportChar.TabIndex = 11;
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(569, 301);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(47, 17);
            this.label5.TabIndex = 12;
            this.label5.Text = "Report";
            // 
            // textBoxWriteData
            // 
            this.textBoxWriteData.Location = new System.Drawing.Point(622, 329);
            this.textBoxWriteData.Name = "textBoxWriteData";
            this.textBoxWriteData.Size = new System.Drawing.Size(150, 24);
            this.textBoxWriteData.TabIndex = 13;
            this.textBoxWriteData.Text = "00 00 00 00";
            // 
            // textBoxReadData
            // 
            this.textBoxReadData.Location = new System.Drawing.Point(622, 359);
            this.textBoxReadData.Name = "textBoxReadData";
            this.textBoxReadData.ReadOnly = true;
            this.textBoxReadData.Size = new System.Drawing.Size(150, 24);
            this.textBoxReadData.TabIndex = 15;
            // 
            // buttonDisconnect
            // 
            this.buttonDisconnect.Location = new System.Drawing.Point(652, 393);
            this.buttonDisconnect.Name = "buttonDisconnect";
            this.buttonDisconnect.Size = new System.Drawing.Size(120, 25);
            this.buttonDisconnect.TabIndex = 17;
            this.buttonDisconnect.Text = "Disconnect";
            this.buttonDisconnect.UseVisualStyleBackColor = true;
            // 
            // buttonDongleReset
            // 
            this.buttonDongleReset.Location = new System.Drawing.Point(652, 424);
            this.buttonDongleReset.Name = "buttonDongleReset";
            this.buttonDongleReset.Size = new System.Drawing.Size(120, 25);
            this.buttonDongleReset.TabIndex = 18;
            this.buttonDongleReset.Text = "Dongle Reset";
            this.buttonDongleReset.UseVisualStyleBackColor = true;
            // 
            // buttonConnect
            // 
            this.buttonConnect.Location = new System.Drawing.Point(408, 11);
            this.buttonConnect.Name = "buttonConnect";
            this.buttonConnect.Size = new System.Drawing.Size(120, 25);
            this.buttonConnect.TabIndex = 20;
            this.buttonConnect.Text = "Connect";
            this.buttonConnect.UseVisualStyleBackColor = true;
            // 
            // buttonWrite
            // 
            this.buttonWrite.Location = new System.Drawing.Point(568, 329);
            this.buttonWrite.Name = "buttonWrite";
            this.buttonWrite.Size = new System.Drawing.Size(48, 25);
            this.buttonWrite.TabIndex = 21;
            this.buttonWrite.Text = "Write";
            this.buttonWrite.UseVisualStyleBackColor = true;
            // 
            // buttonRead
            // 
            this.buttonRead.Location = new System.Drawing.Point(568, 358);
            this.buttonRead.Name = "buttonRead";
            this.buttonRead.Size = new System.Drawing.Size(48, 25);
            this.buttonRead.TabIndex = 22;
            this.buttonRead.Text = "Read";
            this.buttonRead.UseVisualStyleBackColor = true;
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(12, 15);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(84, 17);
            this.label6.TabIndex = 24;
            this.label6.Text = "0 Discovered:";
            // 
            // comboBoxDiscovered
            // 
            this.comboBoxDiscovered.FormattingEnabled = true;
            this.comboBoxDiscovered.Location = new System.Drawing.Point(102, 12);
            this.comboBoxDiscovered.Name = "comboBoxDiscovered";
            this.comboBoxDiscovered.Size = new System.Drawing.Size(300, 25);
            this.comboBoxDiscovered.TabIndex = 23;
            // 
            // buttonScanStop
            // 
            this.buttonScanStop.Location = new System.Drawing.Point(652, 194);
            this.buttonScanStop.Name = "buttonScanStop";
            this.buttonScanStop.Size = new System.Drawing.Size(120, 25);
            this.buttonScanStop.TabIndex = 25;
            this.buttonScanStop.Text = "Scan stop";
            this.buttonScanStop.UseVisualStyleBackColor = true;
            // 
            // checkBoxLesc
            // 
            this.checkBoxLesc.AutoSize = true;
            this.checkBoxLesc.Checked = true;
            this.checkBoxLesc.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxLesc.Location = new System.Drawing.Point(622, 229);
            this.checkBoxLesc.Name = "checkBoxLesc";
            this.checkBoxLesc.Size = new System.Drawing.Size(53, 21);
            this.checkBoxLesc.TabIndex = 26;
            this.checkBoxLesc.Text = "LESC";
            this.checkBoxLesc.UseVisualStyleBackColor = true;
            // 
            // checkBoxOob
            // 
            this.checkBoxOob.AutoSize = true;
            this.checkBoxOob.Location = new System.Drawing.Point(681, 229);
            this.checkBoxOob.Name = "checkBoxOob";
            this.checkBoxOob.Size = new System.Drawing.Size(53, 21);
            this.checkBoxOob.TabIndex = 27;
            this.checkBoxOob.Text = "OOB";
            this.checkBoxOob.UseVisualStyleBackColor = true;
            // 
            // checkBoxMitm
            // 
            this.checkBoxMitm.AutoSize = true;
            this.checkBoxMitm.Location = new System.Drawing.Point(622, 247);
            this.checkBoxMitm.Name = "checkBoxMitm";
            this.checkBoxMitm.Size = new System.Drawing.Size(62, 21);
            this.checkBoxMitm.TabIndex = 28;
            this.checkBoxMitm.Text = "MITM";
            this.checkBoxMitm.UseVisualStyleBackColor = true;
            // 
            // checkBoxKeypress
            // 
            this.checkBoxKeypress.AutoSize = true;
            this.checkBoxKeypress.Location = new System.Drawing.Point(681, 247);
            this.checkBoxKeypress.Name = "checkBoxKeypress";
            this.checkBoxKeypress.Size = new System.Drawing.Size(76, 21);
            this.checkBoxKeypress.TabIndex = 29;
            this.checkBoxKeypress.Text = "Keypress";
            this.checkBoxKeypress.UseVisualStyleBackColor = true;
            // 
            // checkBoxKdistId
            // 
            this.checkBoxKdistId.AutoSize = true;
            this.checkBoxKdistId.Location = new System.Drawing.Point(681, 265);
            this.checkBoxKdistId.Name = "checkBoxKdistId";
            this.checkBoxKdistId.Size = new System.Drawing.Size(38, 21);
            this.checkBoxKdistId.TabIndex = 31;
            this.checkBoxKdistId.Text = "Id";
            this.checkBoxKdistId.UseVisualStyleBackColor = true;
            // 
            // checkBoxKdistEnc
            // 
            this.checkBoxKdistEnc.AutoSize = true;
            this.checkBoxKdistEnc.Checked = true;
            this.checkBoxKdistEnc.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxKdistEnc.Location = new System.Drawing.Point(622, 265);
            this.checkBoxKdistEnc.Name = "checkBoxKdistEnc";
            this.checkBoxKdistEnc.Size = new System.Drawing.Size(47, 21);
            this.checkBoxKdistEnc.TabIndex = 30;
            this.checkBoxKdistEnc.Text = "Enc";
            this.checkBoxKdistEnc.UseVisualStyleBackColor = true;
            // 
            // label7
            // 
            this.label7.AutoSize = true;
            this.label7.Location = new System.Drawing.Point(569, 230);
            this.label7.Name = "label7";
            this.label7.Size = new System.Drawing.Size(35, 17);
            this.label7.TabIndex = 32;
            this.label7.Text = "Auth";
            // 
            // label8
            // 
            this.label8.AutoSize = true;
            this.label8.Location = new System.Drawing.Point(569, 266);
            this.label8.Name = "label8";
            this.label8.Size = new System.Drawing.Size(50, 17);
            this.label8.TabIndex = 33;
            this.label8.Text = "KeyDist";
            // 
            // Form1
            // 
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.None;
            this.ClientSize = new System.Drawing.Size(784, 461);
            this.Controls.Add(this.label8);
            this.Controls.Add(this.label7);
            this.Controls.Add(this.checkBoxKdistId);
            this.Controls.Add(this.checkBoxKdistEnc);
            this.Controls.Add(this.checkBoxKeypress);
            this.Controls.Add(this.checkBoxMitm);
            this.Controls.Add(this.checkBoxOob);
            this.Controls.Add(this.checkBoxLesc);
            this.Controls.Add(this.buttonScanStop);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.comboBoxDiscovered);
            this.Controls.Add(this.buttonRead);
            this.Controls.Add(this.buttonWrite);
            this.Controls.Add(this.buttonConnect);
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
        private System.Windows.Forms.Button buttonConnect;
        private System.Windows.Forms.Button buttonWrite;
        private System.Windows.Forms.Button buttonRead;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.ComboBox comboBoxDiscovered;
        private System.Windows.Forms.Button buttonScanStop;
        private System.Windows.Forms.CheckBox checkBoxLesc;
        private System.Windows.Forms.CheckBox checkBoxOob;
        private System.Windows.Forms.CheckBox checkBoxMitm;
        private System.Windows.Forms.CheckBox checkBoxKeypress;
        private System.Windows.Forms.CheckBox checkBoxKdistId;
        private System.Windows.Forms.CheckBox checkBoxKdistEnc;
        private System.Windows.Forms.Label label7;
        private System.Windows.Forms.Label label8;
    }
}

