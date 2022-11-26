using System.Collections.ObjectModel;
using System.Runtime.Serialization.Formatters.Binary;
using Microsoft.UI.Xaml.Controls;
using WindowsPackageManagerUI.Helpers;
using WindowsPackageManagerUI.ViewModels;

namespace WindowsPackageManagerUI.Views;

public sealed partial class MainPage : Page
{
    private ObservableCollection<TreeNode> DataSource = new ObservableCollection<TreeNode>();
    private SortedDictionary<string, SortedSet<string>> logsCollection = new SortedDictionary<string, SortedSet<string>>();
    private string logFile = "C:\\WindowsPackageManager\\WindowsPackageManagerLog.txt";

    public MainViewModel ViewModel
    {
        get;
    }

    public MainPage()
    {
        ViewModel = App.GetService<MainViewModel>();
        InitializeComponent();
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

        foreach(var log in logsCollection)
        {
            ObservableCollection<TreeNode> children = new ObservableCollection<TreeNode>(); 
            foreach(string file in log.Value)
            {
                children.Add(new TreeNode
                {
                    Name = file,
                    Children = new ObservableCollection<TreeNode>()
                });
            }
            data.Add(new TreeNode
            {
                Name = log.Key,
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
    public ObservableCollection<TreeNode> Children { get; set; } = new ObservableCollection<TreeNode>();

    public override string ToString()
    {
        return Name;
    }
}
