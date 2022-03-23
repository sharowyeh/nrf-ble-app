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

        public Form1()
        {
            InitializeComponent();

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

            buttonDongleInit.Click += ButtonDongleInit_Click;
            buttonScanStart.Click += ButtonScanStart_Click;
            buttonConnect.Click += ButtonConnect_Click;
            buttonDisconnect.Click += ButtonDisconnect_Click;
            buttonDongleReset.Click += ButtonDongleReset_Click;
            buttonDongleClose.Click += ButtonDongleClose_Click;
        }

        private void ButtonDongleInit_Click(object sender, EventArgs e)
        {
            uint baudrate = uint.Parse(textBoxBaudRate.Text);
            var code = NrfBLELibrary.DongleInit(textBoxSerialPort.Text, baudrate);
            WriteLog($"dongle init {(code == 0 ? "ok" : "fail")} code:{code}");
        }

        private void ButtonScanStart_Click(object sender, EventArgs e)
        {
            CleanupData();
            NrfBLELibrary.ScanStart(200, 50, true, 0);
        }

        private void ButtonConnect_Click(object sender, EventArgs e)
        {
            if (discovered == false)
                return;
            
            NrfBLELibrary.ConnStart(foundAddrType, foundAddr);
        }

        private void ButtonDisconnect_Click(object sender, EventArgs e)
        {
            CleanupData();
            NrfBLELibrary.DongleDisconnect();
        }

        private void ButtonDongleReset_Click(object sender, EventArgs e)
        {
            var code = NrfBLELibrary.DongleReset();
            WriteLog($"dongle reset {(code == 0 ? "ok" : "fail")} code:{code}");
        }

        private void ButtonDongleClose_Click(object sender, EventArgs e)
        {
            var code = NrfBLELibrary.DongleClose();
            WriteLog($"dongle close {(code == 0 ? "ok" : "fail")} code:{code}");
        }

        #region Helper functions

        private void WriteLog(string message)
        {
            Console.WriteLine($"[form] {message}");
            var stamp = $"{DateTime.Now:mm:ss.fff}";
            textBoxLog.Invoke(new Action<string>((obj) => {
                textBoxLog.AppendText($"{textBoxLog.Lines.Length} [{obj}] {message}\r\n");
                //if (textBoxLog.Lines.Length > 100)
                //{
                //    var lines = textBoxLog.Lines;
                //    textBoxLog.Lines = lines.Skip(5).Take(textBoxLog.Lines.Length - 5).ToArray();
                //}
            }), stamp);
        }

        private void CleanupData()
        {
            discovered = false;
            foundAddrType = 0;
            foundAddr = new byte[6];
        }

        #endregion


        #region Callback functions

        private bool discovered = false;
        private byte foundAddrType = 0;
        private byte[] foundAddr = new byte[6];

        private void OnDiscovered(string addrString, string name, byte addrType, byte[] addr, sbyte rssi)
        {
            WriteLog($"discovered address:{addrString} rssi:{rssi} type:{addrType}");

            string targetAddrString = (string)textBoxAddress.Invoke(
                new Func<string>(() => { return textBoxAddress.Text; }));
            string targetRssi = (string)textBoxRssi.Invoke(
                new Func<string>(() => { return textBoxRssi.Text; }));

            if (rssi < sbyte.Parse(targetRssi) || discovered)
                return;

            if (string.IsNullOrWhiteSpace(targetAddrString) ||
                addrString.Equals(targetAddrString))
            {
                discovered = true;
                foundAddrType = addrType;
                Array.Copy(addr, foundAddr, 6);
                WriteLog($"=== {addrString} {rssi} match, ready to connect ===");
                NrfBLELibrary.ScanStop();
                ///if auto
                //NrfBLELibrary.ConnStart(addrType, addr);
            }
        }

        private void OnConnected(byte addrType, byte[] addr)
        {
            WriteLog($"connected, auth start");
            NrfBLELibrary.AuthStart(true, false, 0x2);
        }

        private void OnPasskeyRequired(string passkey)
        {
            WriteLog($"=== Show passkey {passkey} ===");
        }

        private int serviceIdx = 0;
        private ushort[][] serviceList = {
            new ushort[] { 0x1800, 0x01 },
            new ushort[] { 0x180F, 0x01 },
            new ushort[] { 0x1812, 0x01 } };

        private void OnAuthenticated(byte status)
        {
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
            comboBoxReportChar.Invoke(new Action<ushort[], byte[], ushort>((h, r, l) => {
                comboBoxReportChar.Items.Clear();
                for (int i = 0; i < l; i++)
                {
                    comboBoxReportChar.Items.Add($"{r[i * 2]:x02} {r[i * 2 + 1]:x02} {h[i]:x04}");
                }
                if (l > 0)
                    comboBoxReportChar.SelectedIndex = 0;
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
}
