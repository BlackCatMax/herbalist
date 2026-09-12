// Source/ProjectHerbalistTests/Private/Tests/TrampleSubsystemTest.cpp
//
// Тропы в мире (2026-09-12) -- UTrampleSubsystem: откуда берутся числа,
// когда ходьба становится штрихом, как заводится показ и что переживает
// сейв. Математика самого поля -- в TrampleFieldTest.cpp.
//
// Тесты идут в editor-мире, где подсистема существует (DoesSupportWorldType
// включает Editor), но не тикает. Часы подменяются SetClockOverride; все
// точки -- в километрах от начала координат, чтобы не пересечься с
// содержимым L_TestDev.

#include "Misc/AutomationTest.h"
#include "Core/World/Trample/TrampleSubsystem.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureVolume.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_UploadIntervalIsOneByteStep,
    "Herbalist.Trample.Subsystem.UploadIntervalIsOneByteStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_UploadIntervalIsOneByteStep::RunTest(const FString& Parameters)
{
    // За такт выгрузки значение спадает ровно на одну ступень RGBA8.
    const float FullClear = 13440.0f;
    const float Interval = UTrampleSubsystem::GetUploadIntervalSeconds(FullClear);
    TestEqual(TEXT("Спад за такт = 1/255"), Interval / FullClear, 1.0f / 255.0f, 0.000001f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_NoVolumeMeansNoWriterPlanes,
    "Herbalist.Trample.Subsystem.NoVolumeMeansNoWriterPlanes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_NoVolumeMeansNoWriterPlanes::RunTest(const FString& Parameters)
{
    // На карте без объёма RVT_Trample (L_TestDev сегодня) поле копится, а
    // плоскостей не заводится -- рисовать им некуда.
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    const FVector2D Point(510000.0, 510000.0);
    Trample->AddStroke(Point - FVector2D(200.0, 0.0), Point + FVector2D(200.0, 0.0), 40.0f);
    Trample->RefreshDisplays(Point);

    TestTrue(TEXT("Поле натоптано"), Trample->GetValueAt(Point) > 0.0f);
    TestEqual(TEXT("Плоскостей нет"), Trample->GetDisplayCount(), 0);

    FinishTrampleTest(Trample);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_NearbyChunkGetsWriterPlane,
    "Herbalist.Trample.Subsystem.NearbyChunkGetsWriterPlane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_NearbyChunkGetsWriterPlane::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    URuntimeVirtualTexture* VirtualTexture = Settings->TrampleVirtualTexture.LoadSynchronous();
    if (!TestNotNull(TEXT("RVT_Trample назначен в Herbalist Settings"), VirtualTexture)) { FinishTrampleTest(Trample); return false; }
    if (!TestNotNull(TEXT("M_RVTWriter назначен в Herbalist Settings"), Settings->TrampleWriterMaterial.LoadSynchronous())) { FinishTrampleTest(Trample); return false; }

    ARuntimeVirtualTextureVolume* Volume = World->SpawnActor<ARuntimeVirtualTextureVolume>();
    if (!TestNotNull(TEXT("Объём RVT заспавнен"), Volume)) { FinishTrampleTest(Trample); return false; }
    Volume->VirtualTextureComponent->SetVirtualTexture(VirtualTexture);

    const float SavedRadius = Settings->ActiveSimulationRadiusMeters;
    Settings->ActiveSimulationRadiusMeters = 100.0f;

    const FVector2D Point(520000.0 + 1000.0, 520000.0 + 1000.0);
    const FIntPoint Coord = FTrampleField::WorldToChunk(Point);
    Trample->AddStroke(Point - FVector2D(200.0, 0.0), Point + FVector2D(200.0, 0.0), 40.0f);
    Trample->RefreshDisplays(Point);

    const FTrampleChunkDisplay* Display = Trample->FindDisplay(Coord);
    if (TestNotNull(TEXT("Чанк рядом со зрителем получил показ"), Display) && TestNotNull(TEXT("Плоскость есть"), Display->WriterPlane.Get()))
    {
        const UStaticMeshComponent* Mesh = Display->WriterPlane->GetStaticMeshComponent();
        TestTrue(TEXT("Плоскость рисует в RVT_Trample"), Mesh->RuntimeVirtualTextures.Contains(VirtualTexture));
        TestTrue(TEXT("В основном проходе не рисуется"), Mesh->VirtualTextureRenderPassType == ERuntimeVirtualTextureMainPassType::Never);

        const FVector2D Center = FTrampleField::GetChunkCenter(Coord);
        TestEqual(TEXT("Плоскость в центре чанка по X"), Display->WriterPlane->GetActorLocation().X, Center.X, 0.01);
        TestEqual(TEXT("Плоскость в центре чанка по Y"), Display->WriterPlane->GetActorLocation().Y, Center.Y, 0.01);
        TestEqual(TEXT("Масштаб = размер чанка / 100 см"), Display->WriterPlane->GetActorScale3D().X, static_cast<double>(FTrampleField::ChunkSizeCm / 100.0f), 0.001);
        TestTrue(TEXT("Плоскость не повёрнута -- иначе UV разойдутся с X/Y"), Display->WriterPlane->GetActorRotation().IsNearlyZero());

        UTexture* Bound = nullptr;
        if (TestNotNull(TEXT("Материал-писатель создан"), Display->Material.Get()))
        {
            Display->Material->GetTextureParameterValue(FHashedMaterialParameterInfo(Settings->TrampleWriterTextureParameter), Bound);
        }
        TestTrue(TEXT("В параметр материала подана текстура чанка"), Bound != nullptr && Bound == Display->Texture.Get());
    }

    // Зритель ушёл дальше радиуса -- показ снят, данные остались.
    Trample->RefreshDisplays(Point + FVector2D(Settings->ActiveSimulationRadiusMeters * 100.0 + FTrampleField::ChunkSizeCm * 2.0, 0.0));
    TestNull(TEXT("Дальний чанк без показа"), Trample->FindDisplay(Coord));
    TestNotNull(TEXT("Но поле чанка на месте"), Trample->GetField().FindChunk(Coord));

    Settings->ActiveSimulationRadiusMeters = SavedRadius;
    FinishTrampleTest(Trample);
    Volume->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleSubsystem_SaveRoundTripKeepsThePath,
    "Herbalist.Trample.Subsystem.SaveRoundTripKeepsThePath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleSubsystem_SaveRoundTripKeepsThePath::RunTest(const FString& Parameters)
{
    // Недельная тропа обязана пережить загрузку. Сохраняется уже распавшееся
    // значение; время между сессиями игровым не считается.
    UWorld* World = nullptr;
    UTrampleSubsystem* Trample = GetTrampleForTest(*this, World);
    if (!Trample) return false;

    const FVector2D Point(530000.0 + 1000.0, 530000.0 + 1000.0);
    Trample->AddStroke(Point - FVector2D(200.0, 0.0), Point + FVector2D(200.0, 0.0), 40.0f);
    Trample->AddStroke(Point - FVector2D(200.0, 0.0), Point + FVector2D(200.0, 0.0), 40.0f);

    const float Later = Trample->GetFullClearSeconds(FTrampleField::WorldToChunk(Point)) * 0.1f;
    Trample->SetClockOverride(Later);
    const float BeforeSave = Trample->GetValueAt(Point);
    TestTrue(TEXT("К моменту сейва тропа частично заросла, но есть"), BeforeSave > 0.0f);

    const TArray<FSavedTrampleChunk> Saved = Trample->CaptureSaveChunks();
    TestEqual(TEXT("Сохранён один чанк"), Saved.Num(), 1);

    Trample->ResetTrample();
    TestEqual(TEXT("После сброса поле пусто"), Trample->GetValueAt(Point), 0.0f, 0.0001f);

    // Загрузка в сессии с другими часами.
    Trample->SetClockOverride(12345.0f);
    Trample->RestoreSaveChunks(Saved);
    TestEqual(TEXT("После загрузки -- то же значение"), Trample->GetValueAt(Point), BeforeSave, 1.0f / 65535.0f + 0.00001f);

    FinishTrampleTest(Trample);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
