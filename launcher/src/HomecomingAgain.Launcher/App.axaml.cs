using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using HomecomingAgain.Launcher.Core;
using HomecomingAgain.Launcher.ViewModels;
using HomecomingAgain.Launcher.Views;

namespace HomecomingAgain.Launcher;

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
