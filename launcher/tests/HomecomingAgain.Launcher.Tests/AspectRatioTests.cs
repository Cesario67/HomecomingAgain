using HomecomingAgain.Launcher.Core;
using HomecomingAgain.Launcher.ViewModels;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

public sealed class AspectRatioTests
{
    [Theory]
    [InlineData(1024, 768, AspectRatio.FourByThree)]
    [InlineData(1280, 1024, AspectRatio.FourByThree)]
    [InlineData(1920, 1080, AspectRatio.SixteenByNine)]
    [InlineData(1920, 1200, AspectRatio.SixteenByNine)]  // 16:10 sits with 16:9
    [InlineData(2560, 1080, AspectRatio.TwentyOneByNine)]
    [InlineData(3440, 1440, AspectRatio.TwentyOneByNine)]
    [InlineData(3840, 1080, AspectRatio.TwentyOneByNine)]  // 32:9 belongs to the widest format
    public void SizesAreClassifiedByTheirRatio(int width, int height, AspectRatio expected) =>
        Assert.Equal(expected, AspectRatios.Classify(width, height));

    [Theory]
    [InlineData(AspectRatio.FourByThree)]
    [InlineData(AspectRatio.SixteenByNine)]
    [InlineData(AspectRatio.TwentyOneByNine)]
    public void EverySizeOfAFormatBelongsToIt(AspectRatio ratio)
    {
        IReadOnlyList<(int Width, int Height)> sizes = AspectRatios.SizesFor(ratio);
        Assert.NotEmpty(sizes);
        Assert.All(sizes, size => Assert.Equal(ratio, AspectRatios.Classify(size.Width, size.Height)));
    }

    [Fact]
    public void ACustomSizeIsKeptOnlyInItsOwnFormat()
    {
        // 1920x810 is not in any list but is 21:9: it must stay selectable there and nowhere else.
        Assert.Contains((1920, 810), AspectRatios.SizesIncluding(AspectRatio.TwentyOneByNine, 1920, 810));
        Assert.DoesNotContain((1920, 810), AspectRatios.SizesIncluding(AspectRatio.SixteenByNine, 1920, 810));
        Assert.Equal(AspectRatios.SizesFor(AspectRatio.FourByThree), AspectRatios.SizesIncluding(AspectRatio.FourByThree, 1920, 810));
    }

    [Theory]
    [InlineData(AspectRatio.TwentyOneByNine, 1080, 2560, 1080)]
    [InlineData(AspectRatio.TwentyOneByNine, 1440, 3440, 1440)]
    [InlineData(AspectRatio.TwentyOneByNine, 2160, 5120, 2160)]
    [InlineData(AspectRatio.SixteenByNine, 1080, 1920, 1080)]
    [InlineData(AspectRatio.SixteenByNine, 1600, 2560, 1440)]
    [InlineData(AspectRatio.FourByThree, 1080, 1400, 1050)]
    public void ChangingFormatKeepsTheClosestHeight(AspectRatio ratio, int previousHeight, int width, int height) =>
        Assert.Equal((width, height), AspectRatios.Closest(AspectRatios.SizesFor(ratio), previousHeight));

    [Theory]
    [InlineData(1920, 1080, false)]
    [InlineData(1920, 1200, false)]
    [InlineData(1024, 768, false)]
    [InlineData(2560, 1080, true)]
    [InlineData(3840, 1080, true)]
    [InlineData(0, 0, false)]
    public void HorPlusOnlyConcernsScreensWiderThanSixteenByNine(int width, int height, bool expected) =>
        Assert.Equal(expected, AspectRatios.IsWiderThanSixteenByNine(width, height));

    [Fact]
    public void TheFormatSelectorDrivesTheResolutionList()
    {
        GraphicsViewModel graphics = new();
        graphics.Load(new PatchSettings { Width = 1920, Height = 1080 });
        Assert.Equal(AspectRatio.SixteenByNine, graphics.SelectedAspectRatio.Value);
        Assert.False(graphics.HorPlusAvailable);
        Assert.False(graphics.HorPlus);

        graphics.SelectedAspectRatio = graphics.AspectRatioChoices.First(choice => choice.Value == AspectRatio.TwentyOneByNine);
        Assert.Equal((2560, 1080), graphics.SelectedResolution.Value);
        Assert.All(graphics.Resolutions, choice => Assert.Equal(AspectRatio.TwentyOneByNine, AspectRatios.Classify(choice.Value.Width, choice.Value.Height)));
        Assert.True(graphics.HorPlusAvailable);
        Assert.True(graphics.HorPlus);  // checked when switching to 21:9

        graphics.SelectedAspectRatio = graphics.AspectRatioChoices.First(choice => choice.Value == AspectRatio.FourByThree);
        Assert.Equal((1400, 1050), graphics.SelectedResolution.Value);
        Assert.False(graphics.HorPlusAvailable);
        Assert.False(graphics.HorPlus);
    }

    [Fact]
    public void ASavedUltrawideSizeSelectsTheWidestFormat()
    {
        GraphicsViewModel graphics = new();
        graphics.Load(new PatchSettings { Width = 1920, Height = 810 });
        Assert.Equal(AspectRatio.TwentyOneByNine, graphics.SelectedAspectRatio.Value);
        Assert.Equal((1920, 810), graphics.SelectedResolution.Value);
        Assert.Contains(graphics.Resolutions, choice => choice.Value == (1920, 810));
    }

    [Theory]
    [InlineData(2560, 1080, true, "1")]
    [InlineData(2560, 1080, false, "0")]
    [InlineData(1920, 1080, true, "0")]  // a stale tick never enables Hor+ on a 16:9 size
    public void HorPlusIsWrittenOnlyForWideSizes(int width, int height, bool tick, string written)
    {
        IniDocument mod = IniDocument.Parse("");
        (new PatchSettings { Width = width, Height = height, HorPlus = tick }).WriteMod(mod);
        Assert.Equal(written, mod.Get("Graphics", "HorPlus"));
    }

    [Fact]
    public void HorPlusIsReadFromTheIni()
    {
        PatchSettings settings = PatchSettings.Read(IniDocument.Parse("[Graphics]\nHorPlus=1\n"), KeyValueDocument.Parse("ScreenResWidth=2560\nScreenResHeight=1080\n"));
        Assert.True(settings.HorPlus);
        Assert.False(PatchSettings.Read(IniDocument.Parse(""), KeyValueDocument.Parse("")).HorPlus);
    }
}
