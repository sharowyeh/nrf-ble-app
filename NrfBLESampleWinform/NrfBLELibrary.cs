﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace NrfBLESampleWinform
{
    public enum FnCallbackId
    {
        FN_ON_DISCOVERED,
        FN_ON_CONNECTED,
        FN_ON_PASSKEY_REQUIRED,
        FN_ON_AUTHENTICATED,
        FN_ON_SERVICE_DISCOVERED,
        FN_ON_SERVICE_ENABLED,
        FN_ON_DISCONNECTED,
        FN_ON_FAILED,
        FN_ON_DATA_RECEIVED,
        FN_ON_DATA_SENT
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnDiscovered(
        [MarshalAs(UnmanagedType.LPStr)]string addrString,
        [MarshalAs(UnmanagedType.LPStr)]string name,
        byte addr_type,
        [MarshalAs(UnmanagedType.LPArray, SizeConst = 6)]byte[] addr,
        sbyte rssi);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnConnected(
        byte addr_type,
        [MarshalAs(UnmanagedType.LPArray, SizeConst = 6)]byte[] addr);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnPasskeyRequired(
        [MarshalAs(UnmanagedType.LPStr)]string passkey);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnAuthenticated(byte status);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnServiceDiscovered(ushort lastHandle, ushort charCount);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnServiceEnabled(ushort enabledCount);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnDisconnected(byte reason);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnFailed(
        [MarshalAs(UnmanagedType.LPStr)]string stage);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnDataReceived(
        ushort handle, 
        [MarshalAs(UnmanagedType.LPArray, SizeConst = 256)]byte[] data, 
        ushort len);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FnOnDataSent(
        ushort handle,
        [MarshalAs(UnmanagedType.LPArray, SizeConst = 256)]byte[] data, 
        ushort len);

    public class NrfBLELibrary
    {
        public const int DATA_BUFFER_SIZE = 256;
        public const int BLE_GAP_ADDR_LEN = 6;

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "callback_add")]
        public static extern uint CallbackAdd(FnCallbackId fnId, IntPtr fnPtr);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "dongle_init")]
        public static extern uint DongleInit(string serialPort, uint baudRate);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "keypair_init")]
        public static extern uint KeypairInit(bool renew);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "scan_start")]
        public static extern uint ScanStart(float interval, float window, bool active, ushort timeout);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "scan_stop")]
        public static extern uint ScanStop();

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "conn_start")]
        public static extern uint ConnStart(byte addrType, 
            [MarshalAs(UnmanagedType.LPArray, SizeConst = 6)]byte[] addr);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "auth_set_params")]
        public static extern uint AuthSetParams(bool lesc, bool oob, bool mitm, byte role, bool enc, bool id, bool sign, bool link);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "auth_start")]
        public static extern uint AuthStart(bool bond, bool keypress, byte ioCaps, 
            [MarshalAs(UnmanagedType.LPStr, SizeConst = 6)]string passkey);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "service_discovery_start")]
        public static extern uint ServiceDiscoveryStart(ushort uuid, byte type);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "service_enable_start")]
        public static extern uint ServiceEnableStart();

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "report_char_list")]
        public static extern uint ReportCharList(
            [MarshalAs(UnmanagedType.LPArray, SizeConst = DATA_BUFFER_SIZE)]ushort[] handle_list, 
            [MarshalAs(UnmanagedType.LPArray, SizeConst = DATA_BUFFER_SIZE * 2)]byte[] refs_list, 
            ref ushort len);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "data_read_async")]
        public static extern uint DataReadAsync(ushort handle);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "data_read")]
        public static extern uint DataRead(ushort handle, 
            [MarshalAs(UnmanagedType.LPArray, SizeConst = DATA_BUFFER_SIZE)]byte[] data, ref ushort len, ushort timeout);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "data_read_by_report_ref")]
        public static extern uint DataReadByReportRef(byte[] reportRef,
            [MarshalAs(UnmanagedType.LPArray, SizeConst = DATA_BUFFER_SIZE)]byte[] data, ref ushort len, ushort timeout);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "data_write_async")]
        public static extern uint DataWriteAsync(ushort handle,
            [MarshalAs(UnmanagedType.LPArray, SizeConst = DATA_BUFFER_SIZE)]byte[] data, ushort len);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "data_write")]
        public static extern uint DataWrite(ushort handle,
            [MarshalAs(UnmanagedType.LPArray, SizeConst = DATA_BUFFER_SIZE)]byte[] data, ushort len, ushort timeout);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "data_write_by_report_ref")]
        public static extern uint DataWriteByReportRef(byte[] reportRef,
            [MarshalAs(UnmanagedType.LPArray, SizeConst = DATA_BUFFER_SIZE)]byte[] data, ushort len, ushort timeout);

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "dongle_disconnect")]
        public static extern uint DongleDisconnect();

        [DllImport("nrf_ble_library.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "dongle_reset")]
        public static extern uint DongleReset();

    }


}
