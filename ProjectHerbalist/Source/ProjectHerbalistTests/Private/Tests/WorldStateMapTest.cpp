// Source/ProjectHerbalistTests/Private/Tests/WorldStateMapTest.cpp
//
// Карта состояния мира в текстуру (2026-09-07, "план A"). Проверяется
// ровно то, что может молча разъехаться и при этом выглядеть работающим:
// раскладка тексель-в-клетку, значения по каналам и согласие формулы UV
// с системой отсчёта симуляции. Сама выгрузка на GPU здесь не проверяется
// и проверена быть не может -- в headless-прогоне RHI нет; это уровень 4
// (ручной PIE) по ENGINE_VERIFICATION_GUIDE.md.
//
// Отдельный довод про тест UVAgreesWithWorldPositionToCell. Материал
// строит UV из мировой позиции сам, и ошибка на полклетки в этой формуле
// дала бы картинку, которая выглядит совершенно правдоподобно -- порча
// просто оказалась бы не там, где она есть в симуляции. Проверять поэтому
// надо не "функция вернула что-то в [0,1]", а СЛЕДСТВИЕ: тексель, в
// который попадает точка, обязан совпасть с клеткой, которую для той же
// точки называет WorldPositionToCell.

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateMap_PixelBufferMatchesCellLayout,
    "Herbalist.WorldStateMap.PixelBufferMatchesCellLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateMap_PixelBufferMatchesCellLayout::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const TArray<FColor> Pixels = Manager->BuildWorldStateMapPixels();

    TestEqual(TEXT("One texel per cell"),
        Pixels.Num(), Manager->GridSizeX * Manager->GridSizeY);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateMap_ChannelsCarryTheDocumentedAxes,
    "Herbalist.WorldStateMap.ChannelsCarryTheDocumentedAxes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateMap_ChannelsCarryTheDocumentedAxes::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Три заведомо разных значения -- чтобы перепутанные местами каналы
    // тест не прошёл. С одинаковыми значениями перестановка R и B была бы
    // невидима.
    const int32 X = 3;
    const int32 Y = 5;
    FGridCell* Cell = Manager->GetCell(X, Y);
    if (!TestNotNull(TEXT("Cell (3,5) exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }

    Cell->State.Meta.Distortion = 0.25f;
    Cell->State.Meta.Corruption = 0.50f;
    Cell->HarvestStress = 0.75f;

    const TArray<FColor> Pixels = Manager->BuildWorldStateMapPixels();
    const int32 Index = Y * Manager->GridSizeX + X;
    if (!TestTrue(TEXT("Index is inside the buffer"), Pixels.IsValidIndex(Index)))
    {
        Manager->Destroy();
        return false;
    }

    // Допуск 1 -- ровно шаг квантования 8 бит, не запас на всякий случай.
    const FColor& Texel = Pixels[Index];
    TestTrue(TEXT("R carries Distortion 0.25"), FMath::Abs(int32(Texel.R) - 64) <= 1);
    TestTrue(TEXT("G carries Corruption 0.50"), FMath::Abs(int32(Texel.G) - 128) <= 1);
    TestTrue(TEXT("B carries HarvestStress 0.75"), FMath::Abs(int32(Texel.B) - 191) <= 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateMap_UVAgreesWithWorldPositionToCell,
    "Herbalist.WorldStateMap.UVAgreesWithWorldPositionToCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateMap_UVAgreesWithWorldPositionToCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const int32 SizeX = Manager->GridSizeX;
    const int32 SizeY = Manager->GridSizeY;
    const float CellSize = Manager->CellSize;

    int32 Checked = 0;
    bool bAllAgree = true;

    // Проходим каждую клетку в трёх точках: почти-угол, центр и почти-край.
    // Края -- там, где ошибка на полклетки перекинула бы точку в соседний
    // тексель; центр -- контроль, что и в спокойном случае всё сходится.
    const float Offsets[] = { 0.01f, 0.5f, 0.99f };
    for (int32 Y = 0; Y < SizeY; ++Y)
    {
        for (int32 X = 0; X < SizeX; ++X)
        {
            for (float Frac : Offsets)
            {
                const FVector WorldPos = Manager->GetActorLocation()
                    + FVector((X + Frac) * CellSize, (Y + Frac) * CellSize, 0.0f);

                int32 SimX = -1, SimY = -1;
                const bool bInGrid = Manager->WorldPositionToCell(WorldPos, SimX, SimY);

                FVector2D UV;
                const bool bInMap = Manager->GetWorldStateMapUV(WorldPos, UV);

                if (bInGrid != bInMap)
                {
                    bAllAgree = false;
                    continue;
                }
                if (!bInGrid) continue;

                // То, что сделает материал: UV -> тексель.
                const int32 TexelX = FMath::FloorToInt(UV.X * SizeX);
                const int32 TexelY = FMath::FloorToInt(UV.Y * SizeY);

                if (TexelX != SimX || TexelY != SimY)
                {
                    bAllAgree = false;
                }
                ++Checked;
            }
        }
    }

    TestTrue(TEXT("Sampled a meaningful number of positions"), Checked > 0);
    TestTrue(TEXT("Every sampled position maps to the same cell in the map and in the simulation"), bAllAgree);

    // Точка заведомо снаружи сетки не должна считаться попавшей в карту --
    // иначе материал красил бы край сетки бесконечно во все стороны.
    FVector2D OutsideUV;
    const FVector Outside = Manager->GetActorLocation() - FVector(CellSize, CellSize, 0.0f);
    TestFalse(TEXT("A position outside the grid is reported as outside the map"),
        Manager->GetWorldStateMapUV(Outside, OutsideUV));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateMap_FrameMatchesGridExtent,
    "Herbalist.WorldStateMap.FrameMatchesGridExtent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateMap_FrameMatchesGridExtent::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FVector Origin;
    FVector2D WorldSize;
    Manager->GetWorldStateMapFrame(Origin, WorldSize);

    TestEqual(TEXT("Frame origin is the manager location"), Origin, Manager->GetActorLocation());
    // Приведение к float явное: FVector2D в UE5 хранит double, а размеры
    // сетки -- float, и без приведения перегрузка TestEqual неоднозначна.
    TestEqual(TEXT("Frame width covers the whole grid"),
        static_cast<float>(WorldSize.X), Manager->GridSizeX * Manager->CellSize);
    TestEqual(TEXT("Frame height covers the whole grid"),
        static_cast<float>(WorldSize.Y), Manager->GridSizeY * Manager->CellSize);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateMap_ScheduledByDefaultOnBeginPlay,
    "Herbalist.WorldStateMap.ScheduledByDefaultOnBeginPlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateMap_ScheduledByDefaultOnBeginPlay::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestEqual(TEXT("Default interval is 1 second"), Manager->WorldStateMapUpdateIntervalSeconds, 1.0f);
    TestTrue(TEXT("Upload timer is scheduled after BeginPlay"),
        Manager->IsWorldStateMapUpdateScheduled());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateMap_ZeroIntervalDisablesIt,
    "Herbalist.WorldStateMap.ZeroIntervalDisablesIt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateMap_ZeroIntervalDisablesIt::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Ручной спавн без BeginPlay -- интервал надо выставить ДО того, как
    // его прочитает BeginPlay (тот же приём, что в GridCorruptionAutoReportTest).
    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        if (AGridWorldManager* Stale = *It) Stale->Destroy();
    }

    AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->WorldStateMapUpdateIntervalSeconds = 0.0f;
    Manager->DispatchBeginPlay();

    TestFalse(TEXT("Zero interval means no upload timer"),
        Manager->IsWorldStateMapUpdateScheduled());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateMap_EndPlayStopsTheTimer,
    "Herbalist.WorldStateMap.EndPlayStopsTheTimer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateMap_EndPlayStopsTheTimer::RunTest(const FString& Parameters)
{
    // Та же регрессия, что у соседнего таймера автоотчёта: без остановки в
    // EndPlay таймер уничтоженного актора мог бы выстрелить в persistent
    // editor-мире между тестами одного прогона.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    if (!TestTrue(TEXT("Precondition: timer is scheduled right after BeginPlay"),
        Manager->IsWorldStateMapUpdateScheduled()))
    {
        Manager->Destroy();
        return false;
    }

    Manager->EndPlay(EEndPlayReason::Destroyed);

    TestFalse(TEXT("EndPlay clears the upload timer"),
        Manager->IsWorldStateMapUpdateScheduled());

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
