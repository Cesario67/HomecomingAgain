namespace HomecomingAgain.Launcher.Core;

/// <summary>Picture format offered by the launcher. The resolution list follows it.</summary>
public enum AspectRatio
{
    FourByThree,
    SixteenByNine,
    TwentyOneByNine,
    ThirtyTwoByNine,
}

/// <summary>Pure rules about picture formats and the resolutions offered for each (tested in AspectRatioTests).</summary>
public static class AspectRatios
{
    // Boundaries on width / height between the formats: 4:3 = 1.33, 16:9 = 1.78, 21:9 = 2.37, 32:9 = 3.56. Anything
    // wider than 2.9 belongs to the widest format, so a saved custom size is never lost.
    private const double FourByThreeLimit = 1.55;
    private const double SixteenByNineLimit = 2.05;
    private const double TwentyOneByNineLimit = 2.9;

    // The mod widens the field of view only above 16:9 by more than 1 % (graphics/HorPlusRules.h, same margin).
    private const double WiderThanSixteenByNineMargin = 1.01;

    private static readonly (int Width, int Height)[] FourByThreeSizes =
        [(1024, 768), (1280, 960), (1400, 1050), (1600, 1200), (1920, 1440), (2048, 1536), (2560, 1920)];

    private static readonly (int Width, int Height)[] SixteenByNineSizes =
        [(1280, 720), (1600, 900), (1920, 1080), (2560, 1440), (3200, 1800), (3840, 2160)];

    // Common ultrawide screens: 2560x1080, 3440x1440, 3840x1600 and 5120x2160.
    private static readonly (int Width, int Height)[] TwentyOneByNineSizes =
        [(2560, 1080), (3440, 1440), (3840, 1600), (5120, 2160)];

    // Super ultrawide screens: 3840x1080, 5120x1440 and 7680x2160.
    private static readonly (int Width, int Height)[] ThirtyTwoByNineSizes =
        [(3840, 1080), (5120, 1440), (7680, 2160)];

    /// <summary>Format a size belongs to (the closest one by its width / height ratio).</summary>
    public static AspectRatio Classify(int width, int height)
    {
        if (height <= 0)
        {
            return AspectRatio.SixteenByNine;
        }
        double ratio = (double)width / height;
        return ratio < FourByThreeLimit ? AspectRatio.FourByThree
             : ratio < SixteenByNineLimit ? AspectRatio.SixteenByNine
             : ratio < TwentyOneByNineLimit ? AspectRatio.TwentyOneByNine
             : AspectRatio.ThirtyTwoByNine;
    }

    /// <summary>The usual sizes of a format, smallest first.</summary>
    public static IReadOnlyList<(int Width, int Height)> SizesFor(AspectRatio ratio) => ratio switch
    {
        AspectRatio.FourByThree => FourByThreeSizes,
        AspectRatio.SixteenByNine => SixteenByNineSizes,
        AspectRatio.TwentyOneByNine => TwentyOneByNineSizes,
        _ => ThirtyTwoByNineSizes,
    };

    /// <summary>
    /// The sizes of a format, plus the given one when it belongs to that format and is not in the usual list (a size
    /// written by hand in vars_pc.cfg stays selectable). Ordered by pixel count.
    /// </summary>
    public static IReadOnlyList<(int Width, int Height)> SizesIncluding(AspectRatio ratio, int width, int height)
    {
        List<(int Width, int Height)> sizes = [.. SizesFor(ratio)];
        if (Classify(width, height) == ratio && !sizes.Contains((width, height)))
        {
            sizes.Add((width, height));
        }
        return sizes.OrderBy(size => (long)size.Width * size.Height).ToList();
    }

    /// <summary>
    /// The size to select when the format changes: the one whose height is closest to the previous height (1080
    /// gives 1920x1080 in 16:9 and 2560x1080 in 21:9), the wider one on a tie.
    /// </summary>
    public static (int Width, int Height) Closest(IReadOnlyList<(int Width, int Height)> sizes, int previousHeight) =>
        sizes.OrderBy(size => Math.Abs(size.Height - previousHeight)).ThenByDescending(size => size.Width).First();

    /// <summary>Is the picture wider than 16:9, where the mod widens the field of view (Hor+)?</summary>
    public static bool IsWiderThanSixteenByNine(int width, int height) =>
        height > 0 && width > height * 16.0 / 9.0 * WiderThanSixteenByNineMargin;
}
