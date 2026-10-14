using HomecomingAgain.Launcher.Core;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

public sealed class ControllerBindsTests
{
    // Extrait de Engine\binds_pc_mjs.cfg (Steam, build 6.30, relevé le 14/10/2026).
    private const string OriginalBinds =
        "setbind 1 COMMAND_CAMERA_X       CONTROLLER 0 AXIS_RX         -1.0 1.0 1.0\r\n" +
        "setbind 1 COMMAND_TURNLEFT       CONTROLLER 0 AXIS_RX         -1.0 0.0 1.0\r\n" +
        "addbind 1 COMMAND_TURNLEFT       KEYBOARD   0 KEY_LEFT        -1.0 1.0 1.0\r\n" +
        "setbind 1 COMMAND_TURNRIGHT      CONTROLLER 0 AXIS_RX          0.0 1.0 1.0\r\n" +
        "addbind 1 COMMAND_TURNRIGHT      KEYBOARD   0 KEY_RIGHT       -1.0 1.0 1.0\r\n" +
        "#setbind 0 COMMAND_TURNLEFT       KEYBOARD   0 KEY_LEFT        -1.0 1.0 1.0\r\n";

    [Fact]
    public void CameraFixFreesTheRightStickAndKeepsKeyboardBinds()
    {
        string applied = ControllerBinds.Apply(OriginalBinds, cameraFix: true);
        string[] lines = applied.Split("\r\n");
        Assert.Contains("setbind 1 COMMAND_TURNLEFT DEVICE_INVALID 0 0", lines);
        Assert.Contains("setbind 1 COMMAND_TURNRIGHT DEVICE_INVALID 0 0", lines);
        Assert.DoesNotContain(lines, line => line.StartsWith("setbind 1 COMMAND_TURN", StringComparison.Ordinal) &&
                                             line.Contains("AXIS_RX", StringComparison.Ordinal));
        Assert.Contains("addbind 1 COMMAND_TURNLEFT       KEYBOARD   0 KEY_LEFT        -1.0 1.0 1.0", lines);
        Assert.Contains("setbind 1 COMMAND_CAMERA_X       CONTROLLER 0 AXIS_RX         -1.0 1.0 1.0", lines);
        Assert.True(ControllerBinds.IsApplied(applied));
        Assert.False(ControllerBinds.IsApplied(OriginalBinds));
    }

    [Fact]
    public void ApplyingTwiceChangesNothing()
    {
        string applied = ControllerBinds.Apply(OriginalBinds, cameraFix: true);
        Assert.Equal(applied, ControllerBinds.Apply(applied, cameraFix: true));
        Assert.Equal(OriginalBinds, ControllerBinds.Apply(OriginalBinds, cameraFix: false));
    }

    [Fact]
    public void DisablingRestoresTheOriginalFile()
    {
        string applied = ControllerBinds.Apply(OriginalBinds, cameraFix: true);
        Assert.Equal(OriginalBinds, ControllerBinds.Apply(applied, cameraFix: false));
    }
}
