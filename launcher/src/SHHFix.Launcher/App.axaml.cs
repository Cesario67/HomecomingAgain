using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using SHHFix.Launcher.Core;
using SHHFix.Launcher.ViewModels;
using SHHFix.Launcher.Views;

namespace SHHFix.Launcher;

public sealed class App : Application
{
    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.MainWindow = new MainWindow
            {
                DataContext = new MainWindowViewModel(AppContext.BaseDirectory, Program.SteamCommand,
                    EmbeddedPackage.Resolve(AppContext.BaseDirectory)),
            };
        }
        base.OnFrameworkInitializationCompleted();
    }
}
