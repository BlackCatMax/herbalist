// Source/ProjectHerbalistTests/Private/Tests/TrampleSubsystemTest.cpp
//
// Тропы в мире (2026-09-12) -- UTrampleSubsystem: откуда берутся числа,
// когда ходьба становится штрихом, как тропа проявляется на картинке, что
// уходит в материал и что переживает сейв. Математика поля и окна -- в
// TrampleFieldTest.cpp и TrampleWindowTest.cpp.
//
// Тесты идут в editor-мире, где подсистема существует (DoesSupportWorldType
// включает Editor), но не тикает. Часы подменяются SetClockOverride; все
// точки -- в километрах от начала координат, чтобы не пересечься с
// содержимым L_TestDev.

#include "Misc/AutomationTest.h"
#include "Core/World/Trample/TrampleSubsystem.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    UTrampleSubsystem* GetTrampleForTest(FAutomationTestBase& Test, UWorld*& OutWorld)
    {
        OutWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!Test.TestNotNull(TEXT("Editor world available"), OutWorld)) return nullptr;
        UTrampleSubsystem* Trample = OutWorld->GetSubsystem<UTrampleSubsystem>();
        if (!Test.TestNotNull(TEXT("UTrampleSubsystem exists in editor world"), Trample)) return nullptr;
        Trample->ResetTrample();
        Trample->SetClockOverride(0.0f);
        return Trample;
    }

    void FinishTrampleTest(UTrampleSubsystem* Trample)
    {
        if (Trample)
        {
            Trample->ResetTrample();
            Trample->ClearClockOverride();
        }
    }

    // Центр мирового текселя -- чтобы значение на оси штриха было ровно
    // вкладом прохода, без поправки на смещение от оси.
    FVector TrampleTexelCenterPoint(double X, double Y)
    {
        return FVector(
            (FTrampleField::WorldToTexel(X) + 0.5) * FTrampleField::TexelSizeCm,
            (FTrampleField::WorldToTexel(Y) + 0.5) * FTrampleField::TexelSizeCm,
            0.0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_OnePassPerDayHoldsThePathSteady,
    "Herbalist.Trample.Subsystem.OnePassPerDayHoldsThePathSteady",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_OnePassPerDayHoldsThePathSteady::RunTest(const FString& Parameters)
{
    // Правило, из которого выведен вклад прохода: проход раз в игровые сутки
    // держит тропу на месте при биоме 1.0. Сутки базового распада снимают
    // DaySeconds / (RecoveryDays x DaySeconds) -- ровно столько проход и
    // обязан добавлять.
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const float DaySeconds = Settings->GameDayMinutes * 60.0f;
    const float BaseClearSeconds = Settings->StressRecoveryGameDays * DaySeconds;

    TestEqual(TEXT("Вклад прохода = сутки распада при биоме 1.0"),
        Trample->GetPassDeposit(), DaySeconds / BaseClearSeconds, 0.00001f);
    TestEqual(TEXT("То есть 1 / StressRecoveryGameDays"),
        Trample->GetPassDeposit(), 1.0f / Settings->StressRecoveryGameDays, 0.00001f);

    FinishTrampleTest(Trample);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_RefreshAndEaseStepsAreOneByteStep,
    "Herbalist.Trample.Subsystem.RefreshAndEaseStepsAreOneByteStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_RefreshAndEaseStepsAreOneByteStep::RunTest(const FString& Parameters)
{
    // Оба такта -- ровно одна ступень RGBA8: чаще картинка не изменится ни
    // на байт, реже -- пропустит видимую ступень.
    const float FullClear = 13440.0f;
    TestEqual(TEXT("Распад за такт пересчёта = 1/255"),
        UTrampleSubsystem::GetDecayRefreshIntervalSeconds(FullClear) / FullClear, 1.0f / 255.0f, 0.000001f);
    TestEqual(TEXT("Догон за такт = 1/255"),
        UTrampleSubsystem::GetEaseStepSeconds() * UTrampleSubsystem::VisualRatePerSecond, 1.0f / 255.0f, 0.000001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_PictureRateMatchesWorldStateMap,
    "Herbalist.Trample.Subsystem.PictureRateMatchesWorldStateMap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_PictureRateMatchesWorldStateMap::RunTest(const FString& Parameters)
{
    // Два числа из разных файлов связаны решением: вся картинка мира
    // проявляется с одной скоростью. Поменять одно -- тест упадёт и заставит
    // решить осознанно, остаются ли они равны.
    const AGridWorldManager* ManagerDefaults = GetDefault<AGridWorldManager>();
    TestEqual(TEXT("Скорость троп = скорость карты состояния мира"),
        UTrampleSubsystem::VisualRatePerSecond, ManagerDefaults->WorldStateMapVisualRatePerSecond, 0.000001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_TestTimeScaleShortensClearTime,
    "Herbalist.Trample.Subsystem.TestTimeScaleShortensClearTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_TestTimeScaleShortensClearTime::RunTest(const FString& Parameters)
{
    // "Для теста можно и быстрее": множитель ускоряет зарастание, а вклад
    // прохода не трогает -- тропа протаптывается теми же проходами, только
    // быстрее зарастает.
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const float SavedScale = Settings->TrampleTestTimeScale;
    const FIntPoint Coord(200, 200);

    Settings->TrampleTestTimeScale = 1.0f;
    const float Normal = Trample->GetFullClearSeconds(Coord);
    const float NormalDeposit = Trample->GetPassDeposit();
    Settings->TrampleTestTimeScale = 60.0f;
    const float Fast = Trample->GetFullClearSeconds(Coord);
    const float FastDeposit = Trample->GetPassDeposit();
    Settings->TrampleTestTimeScale = SavedScale;

    TestEqual(TEXT("Множитель 60 -- зарастание в 60 раз быстрее"), Fast, Normal / 60.0f, 0.01f);
    TestEqual(TEXT("Вклад прохода не зависит от множителя"), FastDeposit, NormalDeposit, 0.00001f);

    FinishTrampleTest(Trample);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_WalkingNeedsGroundDistanceAndNoTeleport,
    "Herbalist.Trample.Subsystem.WalkingNeedsGroundDistanceAndNoTeleport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_WalkingNeedsGroundDistanceAndNoTeleport::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    const FVector Start(500000.0, 500000.0, 0.0);
    const float Radius = 40.0f;

    Trample->FeedWalker(Start, Radius, true);
    TestEqual(TEXT("Первая точка только запоминается"), Trample->GetField().GetChunks().Num(), 0);

    Trample->FeedWalker(Start + FVector(30.0, 0.0, 0.0), Radius, true);
    TestEqual(TEXT("30 см -- меньше шага штриха, поле пусто"), Trample->GetField().GetChunks().Num(), 0);

    Trample->FeedWalker(Start + FVector(60.0, 0.0, 0.0), Radius, true);
    TestTrue(TEXT("60 см -- штрих от первой точки"), Trample->GetValueAt(FVector2D(Start) + FVector2D(30.0, 0.0)) > 0.0f);

    // Прыжок: отсчёт сброшен, приземление не чертит линию по воздуху.
    Trample->FeedWalker(Start + FVector(100.0, 0.0, 0.0), Radius, false);
    Trample->FeedWalker(Start + FVector(300.0, 0.0, 0.0), Radius, true);
    TestEqual(TEXT("Под прыжком не натоптано"), Trample->GetValueAt(FVector2D(Start) + FVector2D(200.0, 0.0)), 0.0f, 0.0001f);

    // Телепорт дальше чанка -- не шаги.
    const FVector Far = Start + FVector(300.0 + FTrampleField::ChunkSizeCm + 100.0, 0.0, 0.0);
    Trample->FeedWalker(Far, Radius, true);
    TestEqual(TEXT("Вдоль телепорта не натоптано"),
        Trample->GetValueAt(FVector2D(Start) + FVector2D(300.0 + FTrampleField::ChunkSizeCm * 0.5, 0.0)), 0.0f, 0.0001f);

    FinishTrampleTest(Trample);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_SinglePassStaysInvisibleSeveralPassesFadeIn,
    "Herbalist.Trample.Subsystem.SinglePassStaysInvisibleSeveralPassesFadeIn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_SinglePassStaysInvisibleSeveralPassesFadeIn::RunTest(const FString& Parameters)
{
    // Весь путь "проход -> картинка" разом: один проход записан, но не виден;
    // после нескольких тропа проступает не рывком, а со скоростью картинки,
    // и приходит ровно к порогу-smoothstep от данных.
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    const FVector Viewer = TrampleTexelCenterPoint(541000.0, 541000.0);
    const FVector2D Point(Viewer);
    auto WalkPass = [&]()
    {
        Trample->AddStroke(Point - FVector2D(300.0, 0.0), Point + FVector2D(300.0, 0.0), 40.0f);
    };

    WalkPass();
    Trample->UpdateDisplay(Viewer, 1000.0f);
    TestEqual(TEXT("Один проход записан в данные"), Trample->GetValueAt(Point), Trample->GetPassDeposit(), 0.0001f);
    TestEqual(TEXT("Но на картинке его нет"), Trample->GetDisplayedAt(Point), 0.0f, 0.000001f);

    WalkPass();
    WalkPass();
    WalkPass();
    Trample->UpdateDisplay(Viewer, 1.0f);
    const float AfterOneSecond = Trample->GetDisplayedAt(Point);
    TestTrue(TEXT("После четырёх проходов тропа начала проступать"), AfterOneSecond > 0.0f);
    TestTrue(FString::Printf(TEXT("Но не рывком: за секунду не больше скорости картинки (%.4f)"), AfterOneSecond),
        AfterOneSecond <= UTrampleSubsystem::VisualRatePerSecond * 1.0f + 0.00001f);

    Trample->UpdateDisplay(Viewer, 1000.0f);
    const float Expected = FTrampleWindow::VisualFromRaw(Trample->GetValueAt(Point), Trample->GetPassDeposit());
    TestEqual(TEXT("Со временем приходит ровно к цели"), Trample->GetDisplayedAt(Point), Expected, 0.0001f);
    TestEqual(TEXT("Четыре прохода -- половина"), Expected, 0.5f, 0.001f);

    FinishTrampleTest(Trample);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_FadeRadiiFollowSimulationRadiusAndWindow,
    "Herbalist.Trample.Subsystem.FadeRadiiFollowSimulationRadiusAndWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_FadeRadiiFollowSimulationRadiusAndWindow::RunTest(const FString& Parameters)
{
    // Затухание начинается на радиусе симуляции и кончается там, где данные
    // окна перестают быть гарантированно верными.
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const float SavedRadius = Settings->ActiveSimulationRadiusMeters;

    TestEqual(TEXT("Конец -- граница верных данных окна"), UTrampleSubsystem::GetFadeEndCm(), FTrampleWindow::ValidRadiusCm, 0.001f);

    Settings->ActiveSimulationRadiusMeters = 100.0f;
    TestEqual(TEXT("Радиус 100 м -- начало на 100 м"), Trample->GetFadeStartCm(), 10000.0f, 0.001f);

    Settings->ActiveSimulationRadiusMeters = -1.0f;
    TestEqual(TEXT("Стриминг выключен -- гасим у самой границы"), Trample->GetFadeStartCm(), UTrampleSubsystem::GetFadeEndCm(), 0.001f);

    Settings->ActiveSimulationRadiusMeters = 500.0f;
    TestEqual(TEXT("Радиус больше окна -- начало не дальше конца"), Trample->GetFadeStartCm(), UTrampleSubsystem::GetFadeEndCm(), 0.001f);

    Settings->ActiveSimulationRadiusMeters = SavedRadius;
    FinishTrampleTest(Trample);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_FrameReachesTheMaterialCollection,
    "Herbalist.Trample.Subsystem.FrameReachesTheMaterialCollection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_FrameReachesTheMaterialCollection::RunTest(const FString& Parameters)
{
    // Материал знает о тропах только через MPC: размер окна, радиусы
    // затухания и позицию игрока. Если параметров нет в коллекции (не
    // запущен -run=TrampleMapSetup), запись в них молча теряется -- тест
    // это и ловит.
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    const FVector Viewer(550000.0, 551000.0, 1234.0);
    Trample->UpdateDisplay(Viewer, 0.1f);

    UMaterialParameterCollection* Collection = GetDefault<UHerbalistSettings>()->TrampleFrameCollection.LoadSynchronous();
    if (TestNotNull(TEXT("TrampleFrameCollection назначен в Herbalist Settings"), Collection))
    {
        const FLinearColor Frame = UKismetMaterialLibrary::GetVectorParameterValue(World, Collection, TEXT("TrampleMapFrame"));
        TestEqual(TEXT("R -- размер окна"), Frame.R, FTrampleWindow::WorldSizeCm, 0.01f);
        TestEqual(TEXT("G -- начало затухания"), Frame.G, Trample->GetFadeStartCm(), 0.01f);
        TestEqual(TEXT("B -- конец затухания"), Frame.B, UTrampleSubsystem::GetFadeEndCm(), 0.01f);

        const FLinearColor Player = UKismetMaterialLibrary::GetVectorParameterValue(World, Collection, TEXT("TramplePlayerPosition"));
        TestEqual(TEXT("Позиция игрока X"), static_cast<double>(Player.R), Viewer.X, 1.0);
        TestEqual(TEXT("Позиция игрока Y"), static_cast<double>(Player.G), Viewer.Y, 1.0);
    }

    FinishTrampleTest(Trample);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_MapTextureIsWindowSizedWrapAndLinear,
    "Herbalist.Trample.Subsystem.MapTextureIsWindowSizedWrapAndLinear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_MapTextureIsWindowSizedWrapAndLinear::RunTest(const FString& Parameters)
{
    // Контракт с материалом. Wrap обязателен: окно адресуется по кругу, с
    // Clamp тропы легли бы полосой по краю. Линейная гамма -- это данные, а
    // не цвет. SRGB совпадает с IsSRGB() -- иначе материал не скомпилирует
    // сэмплер Linear Color (ловушка RT_WorldStateMap).
    UTextureRenderTarget2D* Map = GetDefault<UHerbalistSettings>()->TrampleMap.LoadSynchronous();
    if (!TestNotNull(TEXT("TrampleMap назначен в Herbalist Settings"), Map)) return false;

    TestEqual(TEXT("Ширина = окну"), Map->SizeX, FTrampleWindow::Size);
    TestEqual(TEXT("Высота = окну"), Map->SizeY, FTrampleWindow::Size);
    TestTrue(TEXT("Адресация X -- Wrap"), Map->AddressX == TA_Wrap);
    TestTrue(TEXT("Адресация Y -- Wrap"), Map->AddressY == TA_Wrap);
    TestTrue(TEXT("RGBA8"), Map->RenderTargetFormat == RTF_RGBA8);
    TestTrue(TEXT("Линейная гамма"), Map->bForceLinearGamma != 0);
    TestEqual(TEXT("SRGB совпадает с IsSRGB()"), Map->SRGB != 0, Map->IsSRGB());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_SaveRoundTripKeepsThePathAndSnapsTheDisplay,
    "Herbalist.Trample.Subsystem.SaveRoundTripKeepsThePathAndSnapsTheDisplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_SaveRoundTripKeepsThePathAndSnapsTheDisplay::RunTest(const FString& Parameters)
{
    // Недельная тропа обязана пережить загрузку. Сохраняются данные, а не
    // картинка; после загрузки картинка сразу равна данным -- мир не
    // проявляется из нуля у игрока на глазах.
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    // Центр чанка (166, 166): штрих ±300 см не выходит за его границы, так
    // что сохраняться обязан ровно один чанк. Первая версия теста ставила
    // точку в 187.5 см от границы -- штрих честно задевал соседний чанк.
    const double ChunkCenter = 166.0 * FTrampleField::ChunkSizeCm + FTrampleField::ChunkSizeCm * 0.5;
    const FVector Viewer = TrampleTexelCenterPoint(ChunkCenter, ChunkCenter);
    const FVector2D Point(Viewer);
    for (int32 Pass = 0; Pass < 4; ++Pass)
    {
        Trample->AddStroke(Point - FVector2D(300.0, 0.0), Point + FVector2D(300.0, 0.0), 40.0f);
    }
    Trample->UpdateDisplay(Viewer, 1000.0f);

    const float Later = Trample->GetFullClearSeconds(FTrampleField::WorldToChunk(Point)) * 0.1f;
    Trample->SetClockOverride(Later);
    const float BeforeSave = Trample->GetValueAt(Point);
    TestTrue(TEXT("К моменту сейва тропа частично заросла, но есть"), BeforeSave > Trample->GetPassDeposit());

    const TArray<FSavedTrampleChunk> Saved = Trample->CaptureSaveChunks();
    TestEqual(TEXT("Сохранён один чанк"), Saved.Num(), 1);

    Trample->ResetTrample();
    TestEqual(TEXT("После сброса поле пусто"), Trample->GetValueAt(Point), 0.0f, 0.0001f);
    TestEqual(TEXT("И картинка пуста"), Trample->GetDisplayedAt(Point), 0.0f, 0.000001f);

    // Загрузка в сессии с другими часами.
    Trample->SetClockOverride(12345.0f);
    Trample->RestoreSaveChunks(Saved);
    Trample->UpdateDisplay(Viewer, 0.0f);

    const float AfterLoad = Trample->GetValueAt(Point);
    TestEqual(TEXT("Данные -- те же"), AfterLoad, BeforeSave, 1.0f / 65535.0f + 0.00001f);
    TestEqual(TEXT("Картинка -- сразу равна данным, без проявления"),
        Trample->GetDisplayedAt(Point), FTrampleWindow::VisualFromRaw(AfterLoad, Trample->GetPassDeposit()), 0.0001f);

    FinishTrampleTest(Trample);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
