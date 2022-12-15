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
        private bool threadsCreated = false;


        // ---------======= Extern code =======--------- //
        [DllImport("WindowsPackageManager.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern int StartWindowsPackageManagerLogging();

        [DllImport("WindowsPackageManager.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern int StopWindowsPackageManagerLogging();

        [DllImport("WindowsPackageManager.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern int GetWindowsPackageManagerLog([MarshalAs(UnmanagedType.LPWStr)] StringBuilder log, int maxCharacters);

        [DllImport("FltLib.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern int FilterAttach([MarshalAs(UnmanagedType.LPWStr)] string lpFilterName, [MarshalAs(UnmanagedType.LPWStr)] string lpVolumeName, [MarshalAs(UnmanagedType.LPWStr)] string lpInstanceName, int dwCreatedInstanceNameLength, [MarshalAs(UnmanagedType.LPWStr)] string lpCreatedInstanceName);

        [DllImport("FltLib.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern int FilterLoad([MarshalAs(UnmanagedType.LPWStr)] string lpFilterName);
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
            /*int result = FilterLoad("WindowsPackageManager");

            if (result != 0)
            {
                eventLog.WriteEntry("Failed to load WPM filter! (" + result + ")");
                return;
            }*/

            int result = FilterAttach("WindowsPackageManager", "C:\\", null, 0, null);

            if (result != 0)
            {
                eventLog.WriteEntry("Failed attach filter! (" + result + ")");
                return;
            }

            result = StartWindowsPackageManagerLogging();

            if (result != 0)
            {
                eventLog.WriteEntry("Failed to start logging! (" + result + ")");
                return;
            }

            loggingThread = new Thread(LoggingLoop);
            loggingThread.IsBackground = true;
            loggingThread.Start();

            persistingThread = new Thread(PersistLoop);
            persistingThread.IsBackground = true;
            persistingThread.Start();

            threadsCreated = true;

            eventLog.WriteEntry("Successfully started WindowsPackageManager");
        }

        protected override void OnStop()
        {
            if (threadsCreated)
            {
                stopEvent.Set();
                if (!loggingThread.Join(10000))
                {
                    loggingThread.Abort();
                }

                if (!persistingThread.Join(10000))
                {
                    persistingThread.Abort();
                }
            }

            StopWindowsPackageManagerLogging();
            eventLog.WriteEntry("Successfully stopped WindowsPackageManager");
        }

        private void LoggingLoop()
        {
            int timeOut = 1;
            while (!stopEvent.WaitOne(timeOut))
            {
                StringBuilder logBuilder = new StringBuilder(2048);
                int result = GetWindowsPackageManagerLog(logBuilder, 2048);

                if (result != 0)
                {
                    continue;
                }

                string log = logBuilder.ToString();

                string[] strings = log.Split(':');

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
