namespace NrfBLESampleWinform
{
    partial class FormCmdlet
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
            this.dataGridViewCmdlets = new System.Windows.Forms.DataGridView();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridViewCmdlets)).BeginInit();
            this.SuspendLayout();
            // 
            // dataGridViewCmdlets
            // 
            this.dataGridViewCmdlets.AllowUserToAddRows = false;
            this.dataGridViewCmdlets.AllowUserToDeleteRows = false;
            this.dataGridViewCmdlets.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dataGridViewCmdlets.ColumnHeadersVisible = false;
            this.dataGridViewCmdlets.Location = new System.Drawing.Point(13, 13);
            this.dataGridViewCmdlets.Name = "dataGridViewCmdlets";
            this.dataGridViewCmdlets.ReadOnly = true;
            this.dataGridViewCmdlets.RowHeadersVisible = false;
            this.dataGridViewCmdlets.Size = new System.Drawing.Size(309, 336);
            this.dataGridViewCmdlets.TabIndex = 0;
            // 
            // FormCmdlet
            // 
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.None;
            this.ClientSize = new System.Drawing.Size(334, 361);
            this.Controls.Add(this.dataGridViewCmdlets);
            this.Font = new System.Drawing.Font("Calibri", 10.2F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "FormCmdlet";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.Text = "FormCmdlet";
            ((System.ComponentModel.ISupportInitialize)(this.dataGridViewCmdlets)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.DataGridView dataGridViewCmdlets;
    }
}