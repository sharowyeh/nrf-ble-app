using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NrfBLESampleWinform
{
    public partial class FormCmdlet : Form
    {
        private string cmdletFile = "";
        private BindingSource source = new BindingSource();
        private List<Cmdlet> cmdlets = new List<Cmdlet>();

        public FormCmdlet(string filepath)
        {
            InitializeComponent();

            LoadCmdletFile(filepath);

            source.DataSource = cmdlets;
            dataGridViewCmdlets.DataSource = source;
            dataGridViewCmdlets.Columns.Insert(0, new DataGridViewButtonColumn { Text = "Run", UseColumnTextForButtonValue = true});
            dataGridViewCmdlets.CellClick += DataGridViewCmdlets_CellClick;
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            base.OnFormClosing(e);
            // modify to hide instead of close from x button
            if (e.CloseReason != CloseReason.None &&
                e.CloseReason != CloseReason.UserClosing)
                return;
            this.Hide();
            e.Cancel = true;
        }

        public void LoadCmdletFile(string filepath)
        {
            if (string.IsNullOrWhiteSpace(filepath) || System.IO.File.Exists(filepath) == false)
                return;

            cmdlets.Clear();
            cmdletFile = filepath;
            using (var sr = new System.IO.StreamReader(filepath))
            {
                var line = sr.ReadLine();
                while (line != null)
                {
                    line = line.Trim();
                    if (line.Length > 0 && line.Trim().StartsWith("#") == false)
                    {
                        var cmdlet = Cmdlet.ParseCmdlet(line);
                        if (cmdlet != null)
                            cmdlets.Add(cmdlet);
                    }
                    line = sr.ReadLine();
                }
            }

            source.DataSource = cmdlets;
            source.ResetBindings(false);
        }

        private void DataGridViewCmdlets_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            // only works for the button column
            if (e.ColumnIndex == 0)
            {
                var cmdlet = cmdlets[e.RowIndex];
                Form1.SetCmdlet(cmdlet.Direction, cmdlet.ReportChar, cmdlet.HexString);
            }
        }
    }

    public class Cmdlet
    {
        public string Caption { get; set; }
        
        public string ReportChar { get; set; }
        
        // READ or WRITE
        public string Direction { get; set; }
        
        // 2bytes hex string with whitespace separator
        public string HexString { get; set; }
        
        public Cmdlet(string caption, string reportChar, string direction, string hexString)
        {
            Caption = caption;
            ReportChar = reportChar;
            Direction = direction;
            HexString = hexString;
        }

        public static Cmdlet ParseCmdlet(string cmdletString)
        {
            var sep = cmdletString.Trim().Split(',');
            if (sep.Length < 4)
            {
                return null;   
            }
            var cmdlet = new Cmdlet(sep[0].Trim(), sep[1].Trim(), sep[2].Trim(), sep[3].Trim());
            //TODO: do more data validation here?
            return cmdlet;
        }
    }
}
