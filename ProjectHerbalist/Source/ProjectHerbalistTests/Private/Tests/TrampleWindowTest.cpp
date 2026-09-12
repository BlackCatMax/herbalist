// Source/ProjectHerbalistTests/Private/Tests/TrampleWindowTest.cpp
//
// Окно показа троп (2026-09-12) -- чистая логика FTrampleWindow: адресация
// по кругу совпадает с материалом, окно держит зрителя, порог прячет
// одиночный проход, картинка не прыгает. Доводы -- в шапке TrampleWindow.h.

#include "Misc/AutomationTest.h"
#include "Core/World/Trample/TrampleWindow.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    int32 TrampleWindowTestMod(int32 Value, int32 Divisor)
    {
        return ((Value % Divisor) + Divisor) % Divisor;
    }

    FTrampleWindow::FRect TrampleWindowTestTexel(int32 GX, int32 GY)
    {
        FTrampleWindow::FRect Rect;
        Rect.MinGX = GX;
        Rect.MinGY = GY;
        Rect.Width = 1;
        Rect.Height = 1;
        return Rect;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_WrapIndexMatchesMaterialFrac,
    "Herbalist.Trample.Window.WrapIndexMatchesMaterialFrac",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_WrapIndexMatchesMaterialFrac::RunTest(const FString& Parameters)
{
    // Материал читает тропу как frac(WorldPos / WorldSizeCm) с сэмплером
    // Wrap. Центр мирового текселя обязан попасть ровно в тот тексель
    // текстуры, куда его кладёт CPU, -- иначе тропа ляжет со сдвигом.
    for (int32 Global : { -54321, -1025, -1024, -1, 0, 1, 1023, 1024, 12345 })
    {
        const double WorldCenter = (Global + 0.5) * FTrampleField::TexelSizeCm;
        const double UV = FMath::Frac(WorldCenter / FTrampleWindow::WorldSizeCm);
        const int32 MaterialTexel = FMath::FloorToInt(UV * FTrampleWindow::Size);
        TestEqual(FString::Printf(TEXT("Глобальный тексель %d"), Global), FTrampleWindow::WrapIndex(Global), MaterialTexel);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_FirstRecenterCoversTileAlignedWindow,
    "Herbalist.Trample.Window.FirstRecenterCoversTileAlignedWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_FirstRecenterCoversTileAlignedWindow::RunTest(const FString& Parameters)
{
    FTrampleWindow Window;
    const FVector2D Viewer(123456.0, -98765.0);
    const TArray<FTrampleWindow::FRect> Entered = Window.Recenter(Viewer);

    TestEqual(TEXT("Первый вызов -- один прямоугольник"), Entered.Num(), 1);
    if (Entered.Num() == 1)
    {
        TestEqual(TEXT("На всё окно"), Entered[0].Area(), static_cast<int64>(FTrampleWindow::Size) * FTrampleWindow::Size);
    }
    TestEqual(TEXT("Начало по X кратно тайлу"), TrampleWindowTestMod(Window.GetOrigin().X, FTrampleWindow::TileTexels), 0);
    TestEqual(TEXT("Начало по Y кратно тайлу"), TrampleWindowTestMod(Window.GetOrigin().Y, FTrampleWindow::TileTexels), 0);

    const int32 DriftX = FTrampleField::WorldToTexel(Viewer.X) - (Window.GetOrigin().X + FTrampleWindow::Size / 2);
    const int32 DriftY = FTrampleField::WorldToTexel(Viewer.Y) - (Window.GetOrigin().Y + FTrampleWindow::Size / 2);
    TestTrue(TEXT("Зритель в пределах полутайла от центра"),
        FMath::Abs(DriftX) <= FTrampleWindow::TileTexels / 2 && FMath::Abs(DriftY) <= FTrampleWindow::TileTexels / 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_SmallMovesKeepTheWindow,
    "Herbalist.Trample.Window.SmallMovesKeepTheWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_SmallMovesKeepTheWindow::RunTest(const FString& Parameters)
{
    // Окно не ездит за каждым шагом: иначе полоса в 1024 текселя
    // перезаливалась бы каждые 25 см ходьбы.
    FTrampleWindow Window;
    Window.Recenter(FVector2D::ZeroVector);
    const FIntPoint Origin = Window.GetOrigin();

    const TArray<FTrampleWindow::FRect> Entered = Window.Recenter(FVector2D(20.0 * FTrampleField::TexelSizeCm, -20.0 * FTrampleField::TexelSizeCm));
    TestEqual(TEXT("Сдвиг на 20 текселей -- ничего не вошло"), Entered.Num(), 0);
    TestEqual(TEXT("Окно на месте"), Window.GetOrigin(), Origin);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_RecenterReturnsExactlyTheNewPart,
    "Herbalist.Trample.Window.RecenterReturnsExactlyTheNewPart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_RecenterReturnsExactlyTheNewPart::RunTest(const FString& Parameters)
{
    // Диагональный переезд: вошедшие прямоугольники лежат в новом окне, не
    // задевают старое, не пересекаются между собой и вместе дают ровно
    // новую часть. Лишний пересчёт -- работа впустую, недостающий -- чужие
    // данные из прошлого круга текстуры.
    FTrampleWindow Window;
    Window.Recenter(FVector2D::ZeroVector);
    const FTrampleWindow::FRect OldRect = Window.GetWindowRect();

    const TArray<FTrampleWindow::FRect> Entered = Window.Recenter(FVector2D(100.0 * FTrampleField::TexelSizeCm, -70.0 * FTrampleField::TexelSizeCm));
    const FTrampleWindow::FRect NewRect = Window.GetWindowRect();

    TestTrue(TEXT("Окно переехало"), NewRect.MinGX != OldRect.MinGX || NewRect.MinGY != OldRect.MinGY);
    TestEqual(TEXT("По X -- на целые тайлы"), TrampleWindowTestMod(NewRect.MinGX - OldRect.MinGX, FTrampleWindow::TileTexels), 0);
    TestEqual(TEXT("По Y -- на целые тайлы"), TrampleWindowTestMod(NewRect.MinGY - OldRect.MinGY, FTrampleWindow::TileTexels), 0);

    int64 Sum = 0;
    for (int32 A = 0; A < Entered.Num(); ++A)
    {
        Sum += Entered[A].Area();
        TestEqual(TEXT("Вошедшее -- внутри нового окна"), FTrampleWindow::FRect::Intersect(Entered[A], NewRect).Area(), Entered[A].Area());
        TestEqual(TEXT("Вошедшее -- вне старого окна"), FTrampleWindow::FRect::Intersect(Entered[A], OldRect).Area(), static_cast<int64>(0));
        for (int32 B = A + 1; B < Entered.Num(); ++B)
        {
            TestEqual(TEXT("Полосы не пересекаются"), FTrampleWindow::FRect::Intersect(Entered[A], Entered[B]).Area(), static_cast<int64>(0));
        }
    }
    TestEqual(TEXT("Вместе -- ровно новая часть"), Sum, NewRect.Area() - FTrampleWindow::FRect::Intersect(OldRect, NewRect).Area());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_ViewerAlwaysKeepsTheValidRadius,
    "Herbalist.Trample.Window.ViewerAlwaysKeepsTheValidRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_ViewerAlwaysKeepsTheValidRadius::RunTest(const FString& Parameters)
{
    // Гарантия, на которую опирается затухание в материале: круг
    // ValidRadiusCm вокруг зрителя всегда внутри окна -- при любых шагах,
    // включая телепорты.
    const int32 ValidTexels = FTrampleWindow::Size / 2 - FTrampleWindow::TileTexels;
    TestEqual(TEXT("ValidRadiusCm = (Size/2 - тайл) текселей"), FTrampleWindow::ValidRadiusCm, ValidTexels * FTrampleField::TexelSizeCm, 0.001f);

    FTrampleWindow Window;
    FRandomStream Rng(20260912);
    FVector2D Viewer(50000.0, -30000.0);
    Window.Recenter(Viewer);

    int32 Failures = 0;
    for (int32 Step = 0; Step < 600; ++Step)
    {
        const double Reach = (Step % 97 == 96) ? 200000.0 : 2.0 * FTrampleWindow::TileTexels * FTrampleField::TexelSizeCm;
        Viewer += FVector2D(Rng.FRandRange(-Reach, Reach), Rng.FRandRange(-Reach, Reach));
        Window.Recenter(Viewer);

        const FTrampleWindow::FRect Rect = Window.GetWindowRect();
        const int32 GX = FTrampleField::WorldToTexel(Viewer.X);
        const int32 GY = FTrampleField::WorldToTexel(Viewer.Y);
        if (GX - ValidTexels < Rect.MinGX || GX + ValidTexels > Rect.MinGX + Rect.Width - 1
            || GY - ValidTexels < Rect.MinGY || GY + ValidTexels > Rect.MinGY + Rect.Height - 1)
        {
            ++Failures;
        }
    }
    TestEqual(TEXT("Ни разу круг не вышел за окно"), Failures, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_SinglePassIsInvisibleSeveralPassesAppear,
    "Herbalist.Trample.Window.SinglePassIsInvisibleSeveralPassesAppear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_SinglePassIsInvisibleSeveralPassesAppear::RunTest(const FString& Parameters)
{
    // "Однократный проход совсем не виден -- проявление начинается после
    // нескольких проходов". Порог -- вклад одного прохода.
    const float Pass = 1.0f / 7.0f;
    TestEqual(TEXT("Нетронутое -- ноль"), FTrampleWindow::VisualFromRaw(0.0f, Pass), 0.0f, 0.000001f);
    TestEqual(TEXT("Один проход -- не виден"), FTrampleWindow::VisualFromRaw(Pass, Pass), 0.0f, 0.000001f);
    TestEqual(TEXT("Два -- едва (7%)"), FTrampleWindow::VisualFromRaw(2.0f * Pass, Pass), 0.0740741f, 0.0001f);
    TestEqual(TEXT("Три -- заметно (26%)"), FTrampleWindow::VisualFromRaw(3.0f * Pass, Pass), 0.2592593f, 0.0001f);
    TestEqual(TEXT("Четыре -- половина"), FTrampleWindow::VisualFromRaw(4.0f * Pass, Pass), 0.5f, 0.0001f);
    TestEqual(TEXT("Полная тропа -- единица"), FTrampleWindow::VisualFromRaw(1.0f, Pass), 1.0f, 0.000001f);

    float Previous = 0.0f;
    bool bMonotonic = true;
    for (int32 Step = 0; Step <= 100; ++Step)
    {
        const float Visual = FTrampleWindow::VisualFromRaw(Step / 100.0f, Pass);
        bMonotonic &= (Visual >= Previous);
        Previous = Visual;
    }
    TestTrue(TEXT("Больше натоптано -- не меньше видно"), bMonotonic);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_EasingNeverOutrunsTheRate,
    "Herbalist.Trample.Window.EasingNeverOutrunsTheRate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_EasingNeverOutrunsTheRate::RunTest(const FString& Parameters)
{
    // Против "эффекта бульдозера": цель прыгнула, показ идёт к ней ровно и
    // приходит точно.
    FTrampleWindow Window;
    Window.Recenter(FVector2D::ZeroVector);
    TArray<FIntPoint> Tiles;
    Window.CollectDirtyTiles(Tiles);
    TestEqual(TEXT("Первая выгрузка -- все тайлы"), Tiles.Num(), FTrampleWindow::TilesPerSide * FTrampleWindow::TilesPerSide);

    const FTrampleWindow::FRect Texel = TrampleWindowTestTexel(10, 20);
    Window.SetTargets(Texel, { 1.0f }, 0.0f, false);
    TestEqual(TEXT("Цель задана -- показ ещё на месте"), Window.GetDisplayedAtTexel(10, 20), 0.0f, 0.000001f);
    TestTrue(TEXT("Есть что догонять"), Window.HasEasing());

    Window.Ease(0.01f);
    TestEqual(TEXT("Шаг -- ровно MaxDelta"), Window.GetDisplayedAtTexel(10, 20), 0.01f, 0.000001f);
    Window.CollectDirtyTiles(Tiles);
    TestEqual(TEXT("Байт сменился -- к выгрузке один тайл"), Tiles.Num(), 1);

    int32 Steps = 1;
    while (Window.HasEasing() && Steps < 1000)
    {
        Window.Ease(0.01f);
        ++Steps;
    }
    TestTrue(FString::Printf(TEXT("До цели ~100 шагов (%d)"), Steps), Steps >= 100 && Steps <= 102);
    TestTrue(TEXT("Приходит точно в цель"), Window.GetDisplayedAtTexel(10, 20) == 1.0f);

    Window.CollectDirtyTiles(Tiles);
    Window.Ease(0.01f);
    Window.CollectDirtyTiles(Tiles);
    TestEqual(TEXT("Пришёл -- выгружать нечего"), Tiles.Num(), 0);

    Window.SetTargets(Texel, { 0.0f }, 0.0f, false);
    Window.Ease(0.25f);
    TestEqual(TEXT("Вниз -- с тем же ограничением"), Window.GetDisplayedAtTexel(10, 20), 0.75f, 0.000001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_SnapShowsImmediatelyAndOnlyWhenChanged,
    "Herbalist.Trample.Window.SnapShowsImmediatelyAndOnlyWhenChanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_SnapShowsImmediatelyAndOnlyWhenChanged::RunTest(const FString& Parameters)
{
    FTrampleWindow Window;
    Window.Recenter(FVector2D::ZeroVector);
    TArray<FIntPoint> Tiles;
    Window.CollectDirtyTiles(Tiles);

    Window.SetTargets(TrampleWindowTestTexel(300, -200), { 1.0f }, 0.0f, true);
    TestEqual(TEXT("Вход в окно -- показ сразу равен цели"), Window.GetDisplayedAtTexel(300, -200), 1.0f, 0.000001f);
    TestFalse(TEXT("Догонять нечего"), Window.HasEasing());

    Window.CollectDirtyTiles(Tiles);
    TestEqual(TEXT("К выгрузке -- один тайл"), Tiles.Num(), 1);
    if (Tiles.Num() == 1)
    {
        TestEqual(TEXT("Тот, где лежит тексель"), Tiles[0],
            FIntPoint(FTrampleWindow::WrapIndex(300) / FTrampleWindow::TileTexels, FTrampleWindow::WrapIndex(-200) / FTrampleWindow::TileTexels));
    }

    Window.SetTargets(TrampleWindowTestTexel(300, -200), { 1.0f }, 0.0f, true);
    Window.CollectDirtyTiles(Tiles);
    TestEqual(TEXT("Та же цель ещё раз -- выгружать нечего"), Tiles.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWindow_TilePixelsFollowTextureLayout,
    "Herbalist.Trample.Window.TilePixelsFollowTextureLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWindow_TilePixelsFollowTextureLayout::RunTest(const FString& Parameters)
{
    // Тексель с отрицательным X: в текстуре он у правого края (1021), тайл 15,
    // позиция внутри тайла (61, 8).
    FTrampleWindow Window;
    Window.Recenter(FVector2D::ZeroVector);
    Window.SetTargets(TrampleWindowTestTexel(-3, 200), { 1.0f }, 0.0f, true);

    const FIntPoint Tile(FTrampleWindow::WrapIndex(-3) / FTrampleWindow::TileTexels, FTrampleWindow::WrapIndex(200) / FTrampleWindow::TileTexels);
    TestEqual(TEXT("Тайл по X"), Tile.X, 15);
    TestEqual(TEXT("Тайл по Y"), Tile.Y, 3);

    TArray<FColor> Pixels;
    Window.BuildTilePixels(Tile, Pixels);
    if (!TestEqual(TEXT("64x64 пикселя"), Pixels.Num(), FTrampleWindow::TileTexels * FTrampleWindow::TileTexels)) return false;

    int32 LitCount = 0;
    for (const FColor& Pixel : Pixels)
    {
        LitCount += (Pixel.R > 0) ? 1 : 0;
    }
    TestEqual(TEXT("Светится один пиксель"), LitCount, 1);
    TestEqual(TEXT("Строка 8, столбец 61"), static_cast<int32>(Pixels[8 * FTrampleWindow::TileTexels + 61].R), 255);
    TestEqual(TEXT("Альфа 255"), static_cast<int32>(Pixels[8 * FTrampleWindow::TileTexels + 61].A), 255);
    return true;
}

#endif // WITH_AUTOMATION_TESTS
