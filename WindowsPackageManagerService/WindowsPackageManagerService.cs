using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WindowsPackageManagerService
{
    public partial class WindowsPackageManagerService : ServiceBase
    {
        private static ManualResetEvent stopEvent = new ManualResetEvent(false);
        private static Thread loggingThread;


        // ---------======= Extern code =======--------- //
        [DllImport("WindowsPackageManager.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern int StartWindowsPackageManagerLogging();

        [DllImport("WindowsPackageManager.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern int StopWindowsPackageManagerLogging();

        [DllImport("WindowsPackageManager.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern int GetWindowsPackageManagerLog([MarshalAs(UnmanagedType.LPWStr)] StringBuilder log, int maxCharacters);
        // ---------======= ----------- =======--------- //


        public WindowsPackageManagerService()
        {
            InitializeComponent();
            eventLog = new System.Diagnostics.EventLog();
            if (!System.Diagnostics.EventLog.SourceExists("WindowsPackageManager"))
            {
                System.Diagnostics.EventLog.CreateEventSource(
                    "WindowsPackageManager", "WindowsPackageManagerLog");
            }
            eventLog.Source = "WindowsPackageManager";
            eventLog.Log = "WindowsPackageManagerLog";
        }

        protected override void OnStart(string[] args)
        {
            eventLog.WriteEntry("In OnStart.");
            StartWindowsPackageManagerLogging();

            loggingThread = new Thread(() => LoggingLoop(eventLog));
            loggingThread.IsBackground = true;
            loggingThread.Start();
        }

        protected override void OnStop()
        {
            eventLog.WriteEntry("In OnStop.");

            stopEvent.Set();
            if (!loggingThread.Join(10000))
            {
                loggingThread.Abort();
            }

            StopWindowsPackageManagerLogging();
        }

        private static void LoggingLoop(EventLog eventLog)
        {
            int timeOut = 1;
            while (!stopEvent.WaitOne(timeOut))
            {
                StringBuilder log = new StringBuilder(2048);
                int result = GetWindowsPackageManagerLog(log, 2048);
                eventLog.WriteEntry("Log("+ result.ToString("X")+ "):"+log.ToString());
            }
        }
    }
}
