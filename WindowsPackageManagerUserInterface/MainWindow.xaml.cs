// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Windows.Storage;
using Windows.UI.WindowManagement;
using WindowsPackageManagerUserInterface.Helpers;

namespace WindowsPackageManagerUserInterface;
public sealed partial class MainWindow : Window
{
    private ObservableCollection<TreeNode> DataSource = new ObservableCollection<TreeNode>();
    private SortedDictionary<string, SortedSet<string>> logsCollection = new SortedDictionary<string, SortedSet<string>>();
    private string logFile = "C:\\WindowsPackageManager\\WindowsPackageManagerLog.txt";

    public MainWindow()
    {
        InitializeComponent();

        MicaHelper micaHelper = new MicaHelper(this);
        micaHelper.TrySetSystemBackdrop();

       /* IntPtr hWnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
        var windowId = Microsoft.UI.Win32Interop.GetWindowIdFromWindow(hWnd);
        var appWindow = Microsoft.UI.Windowing.AppWindow.GetFromWindowId(windowId);
        appWindow.Resize(new Windows.Graphics.SizeInt32 { Width = 800, Height = 1200 });*/

        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);

        DataSource = GetData();
    }

    private ObservableCollection<TreeNode> GetData()
    {
        //Format the object as Binary  
        BinaryFormatter formatter = new BinaryFormatter();

        FileStream fs = File.Open(logFile, FileMode.Open);
        logsCollection = (SortedDictionary<string, SortedSet<string>>)formatter.Deserialize(fs);
        fs.Flush();
        fs.Close();
        fs.Dispose();

        ObservableCollection<TreeNode> data = DataSource;
        data.Clear();

        foreach (var log in logsCollection)
        {
            ObservableCollection<TreeNode> children = new ObservableCollection<TreeNode>();
            foreach (string file in log.Value)
            {
                children.Add(new TreeNode
                {
                    Name = file.Split('\\').Last<string>(),
                    Path = file,
                    Children = new ObservableCollection<TreeNode>()
                });
            }
            data.Add(new TreeNode
            {
                Name = log.Key.Split('\\').Last<string>(),
                Path = log.Key,
                Children = children,
            });
        }

        return data;
    }
}

public class TreeNode
{
    public string Name
    {
        get; set;
    }

    public string Path
    {
        get; set;
    }

    public ObservableCollection<TreeNode> Children { get; set; } = new ObservableCollection<TreeNode>();

    public override string ToString()
    {
        return Name;
    }
}
