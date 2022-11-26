﻿using Microsoft.UI.Xaml.Controls;

using WindowsPackageManagerUI.ViewModels;

namespace WindowsPackageManagerUI.Views;

public sealed partial class MainPage : Page
{
    public MainViewModel ViewModel
    {
        get;
    }

    public MainPage()
    {
        ViewModel = App.GetService<MainViewModel>();
        InitializeComponent();
    }
}
