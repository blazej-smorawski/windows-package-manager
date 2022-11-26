using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Diagnostics.Eventing.Reader;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Runtime.Serialization.Formatters.Binary;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WindowsPackageManagerService
{
    public partial class WindowsPackageManagerService : ServiceBase
    {
        private SortedDictionary<string, SortedSet<string>> logsCollection = new SortedDictionary<string, SortedSet<string>>();
        private static ManualResetEvent stopEvent = new ManualResetEvent(false);
        private static Thread loggingThread;
        private static Thread persistingThread;
        private string logFile = "C:\\WindowsPackageManager\\WindowsPackageManagerLog.txt";


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

            loggingThread = new Thread(LoggingLoop);
            loggingThread.IsBackground = true;
            loggingThread.Start();

            persistingThread = new Thread(PersistLoop);
            persistingThread.IsBackground = true;
            persistingThread.Start();
        }

        protected override void OnStop()
        {
            eventLog.WriteEntry("In OnStop.");

            stopEvent.Set();
            if (!loggingThread.Join(10000))
            {
                loggingThread.Abort();
            }

            if (!persistingThread.Join(10000))
            {
                persistingThread.Abort();
            }

            StopWindowsPackageManagerLogging();
        }

        private void LoggingLoop()
        {
            int timeOut = 1;
            while (!stopEvent.WaitOne(timeOut))
            {
                StringBuilder logBuilder = new StringBuilder(2048);
                int result = GetWindowsPackageManagerLog(logBuilder, 2048);

                string log = logBuilder.ToString();
                eventLog.WriteEntry("Log("+ result.ToString("X")+ "):"+log);

                string[] strings = log.Split(':');
                eventLog.WriteEntry(strings[0] + " " + strings[1] + " " + strings[2]);

                SortedSet<string> files;
                if (logsCollection.TryGetValue(strings[2], out files))
                {
                    files.Add(strings[0]);
                } 
                else
                {
                    files = new SortedSet<string>();
                    files.Add(strings[0]);
                    logsCollection.Add(strings[2], files);
                }
            }
        }

        private void PersistLoop()
        {
            int timeOut = 1000;
            eventLog.WriteEntry("PersistLoop");
            while (!stopEvent.WaitOne(timeOut))
            {
                eventLog.WriteEntry("Writing `logsCollection` into: "+logFile);
                Stream ms = File.OpenWrite(logFile);

                BinaryFormatter formatter = new BinaryFormatter();  
                formatter.Serialize(ms, logsCollection);
                ms.Flush();
                ms.Close();
                ms.Dispose();
            }
        }
    }
}
