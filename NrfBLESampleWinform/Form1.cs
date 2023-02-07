using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NrfBLESampleWinform
{
    public partial class Form1 : Form
    {
        public FnOnDiscovered fnOnDiscovered;
        public FnOnConnected fnOnConnected;
        public FnOnPasskeyRequired fnOnPasskeyRequired;
        public FnOnAuthenticated fnOnAuthenticated;
        public FnOnServiceDiscovered fnOnServiceDiscovered;
        public FnOnServiceEnabled fnOnServiceEnabled;
        public FnOnDisconnected fnOnDisconnected;
        public FnOnFailed fnOnFailed;
        public FnOnDataReceived fnOnDataReceived;
        public FnOnDataSent fnOnDataSent;
        
        private bool discovered = false;
        private BindingList<AdvertisingData> discoveredList = new BindingList<AdvertisingData>();
        private AdvertisingData targetDevice = null;

        private int serviceIdx = 0;
        private ushort[][] serviceList = {
            new ushort[] { 0x1800, 0x01 },
            new ushort[] { 0x180F, 0x01 },
            new ushort[] { 0x1812, 0x01 } };

        private bool connected = false;
        private bool authenticated = false;

        private static Form1 inst;
        private FormCmdlet cmdlet = new FormCmdlet("");

        public static void SetCmdlet(string direction, string reportChar, string hexString)
        {
            if (inst == null)
                return;

            // make sure in UI thread
            inst.Invoke(new Action(() =>
            {
                var comboReportChar = inst.comboBoxWriteReportChar;
                var textData = inst.textBoxWriteData;
                var button = inst.buttonWrite;
                if (direction == "READ")
                {
                    comboReportChar = inst.comboBoxReadReportChar;
                    textData = inst.textBoxReadData;
                    button = inst.buttonRead;
                }

                foreach (var i in comboReportChar.Items)
                {
                    if (i is string && ((string)i).Contains(reportChar))
                    {
                        comboReportChar.SelectedItem = i;
                        textData.Text = hexString;
                        button.PerformClick();
                        break;
                    }
                }
            }));
        }

        public Form1()
        {
            InitializeComponent();

            inst = this;

            // binding discovered list to items of combobox control
            var binding = new BindingSource();
            binding.DataSource = discoveredList;
            comboBoxDiscovered.DataSource = binding.DataSource;
            comboBoxDiscovered.DisplayMember = "Display";

            // assign function to delegate field
            fnOnDiscovered = OnDiscovered;
            fnOnConnected = OnConnected;
            fnOnPasskeyRequired = OnPasskeyRequired;
            fnOnAuthenticated = OnAuthenticated;
            fnOnServiceDiscovered = OnServiceDiscovered;
            fnOnServiceEnabled = OnServiceEnabled;
            fnOnDisconnected = OnDisconnected;
            fnOnFailed = OnFailed;
            fnOnDataReceived = OnDataReceived;
            fnOnDataSent = OnDataSent;

            // set callback pointer to dll
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_DISCOVERED, Marshal.GetFunctionPointerForDelegate(fnOnDiscovered));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_CONNECTED, Marshal.GetFunctionPointerForDelegate(fnOnConnected));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_PASSKEY_REQUIRED, Marshal.GetFunctionPointerForDelegate(fnOnPasskeyRequired));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_AUTHENTICATED, Marshal.GetFunctionPointerForDelegate(fnOnAuthenticated));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_SERVICE_DISCOVERED, Marshal.GetFunctionPointerForDelegate(fnOnServiceDiscovered));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_SERVICE_ENABLED, Marshal.GetFunctionPointerForDelegate(fnOnServiceEnabled));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_DISCONNECTED, Marshal.GetFunctionPointerForDelegate(fnOnDisconnected));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_FAILED, Marshal.GetFunctionPointerForDelegate(fnOnFailed));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_DATA_RECEIVED, Marshal.GetFunctionPointerForDelegate(fnOnDataReceived));
            NrfBLELibrary.CallbackAdd(FnCallbackId.FN_ON_DATA_SENT, Marshal.GetFunctionPointerForDelegate(fnOnDataSent));

            ///if auto
            //NrfBLELibrary.DongleInit("COM3", 10000);
            //NrfBLELibrary.ScanStart(200, 50, true, 0);

            comboBoxDiscovered.SelectedIndexChanged += ComboBoxDiscovered_SelectedIndexChanged;
            comboBoxDiscovered.SelectedValueChanged += ComboBoxDiscovered_SelectedIndexChanged;
            
            buttonDongleInit.Click += ButtonDongleInit_Click;
            buttonRenewKeypair.Click += ButtonRenewKeypair_Click;
            buttonScanStart.Click += ButtonScanStart_Click;
            buttonScanStop.Click += ButtonScanStop_Click;
            buttonConnect.Click += ButtonConnect_Click;
            buttonWrite.Click += ButtonWrite_Click;
            buttonRead.Click += ButtonRead_Click;
            buttonDisconnect.Click += ButtonDisconnect_Click;
            buttonDongleReset.Click += ButtonDongleReset_Click;

            comboBoxIoCaps.Items.Add("Display Only");  // BLE_GAP_IO_CAPS_DISPLAY_ONLY  0x00
            comboBoxIoCaps.Items.Add("Display YesNo"); // BLE_GAP_IO_CAPS_DISPLAY_YESNO 0x01
            comboBoxIoCaps.Items.Add("Keyboard Only"); // BLE_GAP_IO_CAPS_KEYBOARD_ONLY 0x02
            comboBoxIoCaps.Items.Add("None");          // BLE_GAP_IO_CAPS_NONE 0x03
            comboBoxIoCaps.Items.Add("Kbd + Display"); // BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY 0x04
            comboBoxIoCaps.SelectedIndex = 2;

            // given cmdlet dataset file, csv type separated by comma, and using sharp# for comments
            var files = System.IO.Directory.GetFiles(".", "*.csv");
            comboBoxCmdletFiles.Items.Clear();
            comboBoxCmdletFiles.Items.AddRange(files);
            if (comboBoxCmdletFiles.Items.Count > 0)
                comboBoxCmdletFiles.SelectedIndex = 0;
            buttonCmdlet.Click += ButtonCmdlet_Click;

            this.FormClosing += Form1_FormClosing;
        }

        private void ButtonCmdlet_Click(object sender, EventArgs e)
        {
            //TODO: check ble connection before open cmdlet form

            var filepath = "";
            if (comboBoxCmdletFiles.SelectedIndex >= 0)
            {
                filepath = comboBoxCmdletFiles.SelectedItem.ToString();
                cmdlet.LoadCmdletFile(filepath);
                cmdlet.Show(this);
            }
            else
            {
                MessageBox.Show($"No cmdlet file present:{filepath}");
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            NrfBLELibrary.DongleDisconnect();
            NrfBLELibrary.DongleReset();
        }

        private void ComboBoxDiscovered_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBoxDiscovered.SelectedIndex == -1)
            {
                targetDevice = null;
                return;
            }

            var selected = comboBoxDiscovered.SelectedItem as AdvertisingData;
            if (targetDevice == null || targetDevice.AddrString != selected.AddrString)
            {
                targetDevice = selected;
                WriteLog($"target {targetDevice.AddrString} selected");
            }
        }

        private void ButtonDongleInit_Click(object sender, EventArgs e)
        {
            WriteLog("dongle init, wait...");
            uint baudrate = uint.Parse(textBoxBaudRate.Text);
            var code = NrfBLELibrary.DongleInit(textBoxSerialPort.Text, baudrate);
            WriteLog($"dongle init {(code == 0 ? "ok" : "fail")} code:{code}");
        }

        private void ButtonRenewKeypair_Click(object sender, EventArgs e)
        {
            var result = MessageBox.Show("key store will be changed, ok or cancel?", "keypair init", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning);
            if (result != DialogResult.OK)
                return;

            WriteLog("renew keypair, wait...");
            NrfBLELibrary.KeypairInit(true);
            WriteLog("keypair updated, check .spk binary");
        }

        private void ButtonScanStart_Click(object sender, EventArgs e)
        {
            CleanupData();
            var code = NrfBLELibrary.ScanStart(200, 50, true, 0);
            WriteLog($"scan start code:{code}, wait...");
        }

        private void ButtonScanStop_Click(object sender, EventArgs e)
        {
            var code = NrfBLELibrary.ScanStop();
            WriteLog($"scan stop code:{code}");
        }

        private void ButtonConnect_Click(object sender, EventArgs e)
        {
            if (targetDevice == null)
                return;

            // For manually connect by combobox selection,
            // raise discovered flag prevent discovery callback changing target device
            discovered = true;

            var code = NrfBLELibrary.ConnStart(targetDevice.AddrType, targetDevice.Addr);
            WriteLog($"connect {targetDevice.AddrString} start code:{code}, wait...");
        }

        private void ButtonWrite_Click(object sender, EventArgs e)
        {
            if (connected == false || authenticated == false || comboBoxWriteReportChar.SelectedItem == null)
            {
                WriteLog("No device connected or report endpoint not selected");
                return;
            }

            byte[] data;
            try
            {
                data = textBoxWriteData.Text.Split(' ').Select(d =>
                {
                    return byte.Parse(d, System.Globalization.NumberStyles.HexNumber);
                }).ToArray();
            }
            catch (Exception)
            {
                WriteLog("Data input format error");
                return;
            }

            uint res = 0xff;
            var args = ((string)comboBoxWriteReportChar.SelectedItem).Split(' ');
            //WriteLog($"data write res:{res}");
            if (args.Length == 3)
            {
                byte[] refs = new byte[2] {
                    byte.Parse(args[0], System.Globalization.NumberStyles.HexNumber),
                    byte.Parse(args[1], System.Globalization.NumberStyles.HexNumber),
                };
                res = NrfBLELibrary.DataWriteByReportRef(refs, data, (ushort)data.Length, 2000);
            }
            // for previous csharp sample version only list endpoint handles
            else if (args.Length == 1)
            {
                var handle = ushort.Parse(args[0], System.Globalization.NumberStyles.HexNumber);
                res = NrfBLELibrary.DataWrite(handle, data, (ushort)data.Length, 2000);
            }

            WriteLog($"data write res:{res}");
        }

        private void ButtonRead_Click(object sender, EventArgs e)
        {
            if (connected == false || authenticated == false || comboBoxReadReportChar.SelectedItem == null)
            {
                WriteLog("No device connected or report endpoint not selected");
                return;
            }

            byte[] data = new byte[NrfBLELibrary.DATA_BUFFER_SIZE];
            ushort len = NrfBLELibrary.DATA_BUFFER_SIZE;

            uint res = 0xff;
            var args = ((string)comboBoxReadReportChar.SelectedItem).Split(' ');
            //WriteLog($"data read res:{res}");
            if (args.Length == 3)
            {
                byte[] refs = new byte[2] {
                    byte.Parse(args[0], System.Globalization.NumberStyles.HexNumber),
                    byte.Parse(args[1], System.Globalization.NumberStyles.HexNumber),
                };
                res = NrfBLELibrary.DataReadByReportRef(refs, data, ref len, 2000);
            }
            // for previous csharp sample version only list endpoint handles
            else if (args.Length == 1)
            {
                var handle = ushort.Parse(args[0], System.Globalization.NumberStyles.HexNumber);
                res = NrfBLELibrary.DataRead(handle, data, ref len, 2000);
            }

            textBoxReadData.Text = "";
            var log = $"data read res:{res} data: ";
            for (int i = 0; i < len; i++)
            {
                log += $"{data[i]:x02} ";
                if (res == 0)
                    textBoxReadData.Text += $"{data[i]:x02} ";
            }
            WriteLog(log);
        }

        private void ButtonDisconnect_Click(object sender, EventArgs e)
        {
            CleanupData();
            var code = NrfBLELibrary.DongleDisconnect();
            WriteLog($"dongle disconnect code:{code}");
        }

        private void ButtonDongleReset_Click(object sender, EventArgs e)
        {
            var code = NrfBLELibrary.DongleReset();
            WriteLog($"dongle reset & close {(code == 0 ? "ok" : "fail")} code:{code}");
        }


        #region Helper functions

        private void WriteLog(string message)
        {
            Console.WriteLine($"[form] {message}");
            var stamp = $"{DateTime.Now:mm:ss.fff}";
            textBoxLog.Invoke(new Action<string>((obj) => {
                textBoxLog.AppendText($"{textBoxLog.Lines.Length} [{obj}] {message}\r\n");
            }), stamp);
        }

        private void CleanupData()
        {
            discovered = false;
            targetDevice = null;
            connected = false;
            authenticated = false;
            comboBoxDiscovered.Invoke(new Action(() =>
            {
                label6.Text = "0 Discovered:";
                discoveredList.Clear();
                comboBoxWriteReportChar.Items.Clear();
            }));
        }

        #endregion


        #region Callback functions

        private void OnDiscovered(string addrString, string name, byte addrType, byte[] addr, sbyte rssi)
        {
            //WriteLog($"discovered {addrString} {name} rssi:{rssi} type:{addrType}");
            comboBoxDiscovered.Invoke(new Action(() =>
            {
                var exists = discoveredList.Where(d => d.AddrString.Equals(addrString));
                if (exists.Count() > 0)
                {
                    if (string.IsNullOrEmpty(name) == false)
                        exists.First().Name = name;
                    exists.First().Rssi = rssi;
                }
                else
                {
                    discoveredList.Add(new AdvertisingData(addrString, name, addrType, addr, rssi));
                }
                label6.Text = $"{discoveredList.Count} Discovered:";
            }));

            string targetAddrString = (string)textBoxAddress.Invoke(
                new Func<string>(() => { return textBoxAddress.Text; }));
            string targetRssi = (string)textBoxRssi.Invoke(
                new Func<string>(() => { return textBoxRssi.Text; }));
            
            if (rssi < sbyte.Parse(targetRssi) || discovered)
                return;

            if (targetAddrString.Length < 12 || addrString.Equals(targetAddrString))
            {
                var code = NrfBLELibrary.ScanStop();
                WriteLog($"target found, scan stop code:{code}");
                discovered = true;

                comboBoxDiscovered.Invoke(new Action(() =>
                {
                    targetDevice = discoveredList.Where(d => d.AddrString.Equals(addrString)).First();
                    comboBoxDiscovered.SelectedItem = targetDevice;
                }));

                WriteLog($"=== {addrString} {name} {rssi} match, ready to connect ===");
                ///if auto
                //NrfBLELibrary.ConnStart(addrType, addr);
            }
        }

        private void OnConnected(byte addrType, byte[] addr)
        {
            connected = true;
            WriteLog($"connected, auth set param");
            // invoke to UI thread for authentication settings
            this.Invoke(new Action(() =>
            {
                // get check box status for AuthSetParams
                NrfBLELibrary.AuthSetParams(checkBoxLesc.Checked, checkBoxOob.Checked, checkBoxMitm.Checked,
                    0, checkBoxKdistEnc.Checked, checkBoxKdistId.Checked, false, false);
                NrfBLELibrary.AuthSetParams(checkBoxLesc.Checked, checkBoxOob.Checked, checkBoxMitm.Checked,
                    1, checkBoxKdistEnc.Checked, checkBoxKdistId.Checked, false, false);

                var validPasskey = textBoxPasskey.Text.Trim().Substring(0, 6).PadRight(6, '0');
                validPasskey = System.Text.RegularExpressions.Regex.Replace(validPasskey, "[^\\d+$]", "0");
                textBoxPasskey.Text = validPasskey;
                WriteLog($"auth start, specify passkey {validPasskey}");
                NrfBLELibrary.AuthStart(checkBoxBond.Checked, checkBoxKeypress.Checked,
                    (byte)comboBoxIoCaps.SelectedIndex, validPasskey);
            }));
        }

        private void OnPasskeyRequired(string passkey)
        {
            WriteLog($"=== Show passkey {passkey} ===");
        }

        private void OnAuthenticated(byte status)
        {
            if (status != 0)
            {
                WriteLog($"auth error code:{status}");
                return;
            }
            authenticated = true;
            serviceIdx = 0;
            WriteLog($"service discovery 0 start");
            NrfBLELibrary.ServiceDiscoveryStart(serviceList[0][0], (byte)serviceList[0][1]);
        }

        private void OnServiceDiscovered(ushort lastHandle, ushort charCount)
        {
            if (++serviceIdx < serviceList.Count())
            {
                WriteLog($"service discovery {serviceIdx} start");
                NrfBLELibrary.ServiceDiscoveryStart(serviceList[serviceIdx][0], (byte)serviceList[serviceIdx][1]);
            }
            else
            {
                WriteLog($"service enable start for total {charCount} characteristics");
                NrfBLELibrary.ServiceEnableStart();
            }
        }

        private void OnServiceEnabled(ushort enabledCount)
        {
            WriteLog($"service enabled with {enabledCount} characteristics");

            ushort[] handle_list = new ushort[256];
            byte[] refs_list = new byte[512];
            ushort len = 256;
            NrfBLELibrary.ReportCharList(handle_list, refs_list, ref len);
            comboBoxWriteReportChar.Invoke(new Action<ushort[], byte[], ushort>((h, r, l) => {
                comboBoxWriteReportChar.Items.Clear();
                comboBoxReadReportChar.Items.Clear();
                for (int i = 0; i < l; i++)
                {
                    if (r[i * 2] == 0 && r[i * 2 + 1] == 0)
                    {
                        comboBoxWriteReportChar.Items.Add($"{h[i]:x04}");
                        comboBoxReadReportChar.Items.Add($"{h[i]:x04}");
                    }
                    else
                    {
                        comboBoxWriteReportChar.Items.Add($"{r[i * 2]:x02} {r[i * 2 + 1]:x02} {h[i]:x04}");
                        comboBoxReadReportChar.Items.Add($"{r[i * 2]:x02} {r[i * 2 + 1]:x02} {h[i]:x04}");
                    }
                }
                if (l > 0)
                {
                    comboBoxWriteReportChar.SelectedIndex = 0;
                    comboBoxReadReportChar.SelectedIndex = 0;
                }
            }), handle_list, refs_list, len);
            
            WriteLog("=== ready to inteact via report characteristic(use hex) ===");
            WriteLog("drop down combo box item to select report characteristic");
            WriteLog("use read/write button with text box");
        }

        private void OnDisconnected(byte reason)
        {
            CleanupData();
            WriteLog($"disconnected reason:{reason}");
        }

        private void OnFailed(string stage)
        {
            CleanupData();
            WriteLog($"on failed {stage}");
        }

        private void OnDataReceived(ushort handle, byte[] data, ushort len)
        {
            var log = $"data received 0x{handle:X} ";
            for (int i = 0; i < len; i++)
            {
                log += $"{data[i]:x02} ";
            }
            WriteLog(log);
        }

        private void OnDataSent(ushort handle, byte[] data, ushort len)
        {
            WriteLog($"data sent 0x{handle:X}");
        }

        #endregion

    }

    public class AdvertisingData
    {
        public string AddrString { get; set; }
        public string Name { get; set; }
        public byte AddrType { get; set; }
        public byte[] Addr { get; set; }
        public sbyte Rssi { get; set; }
        public string Display
        {
            get
            {
                return $"{(AddrString == null ? "" : AddrString)} {(Name == null ? "" : Name)} {Rssi}";
            }
        }
        public AdvertisingData(string addrString, string name, byte addrType, byte[] addr, sbyte rssi)
        {
            AddrString = addrString;
            Name = name;
            AddrType = addrType;
            Addr = new byte[6];
            Array.Copy(addr, Addr, 6);
            Rssi = rssi;
        }
    }
}
