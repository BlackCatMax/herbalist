// Source/ProjectHerbalistTests/Private/Tests/AmbientSpawnerTest.cpp
//
// Спавнеры Низших, этап 1 (DESIGN_Entity_Spawners.md, решения пользователя
// 2026-09-20): спавнер выбирает ОДИН вид по клетке своего центра и выпускает
// бродящих особей; ручной спавнер отменяет автоматические в своём радиусе;
// с включёнными спавнерами клеточный путь Низших молчит. Эффекта на клетки у
// особей на этом этапе нет -- он этапа 2.
//
// Все тесты возвращают bUseAmbientSpawners обратно: настройка живёт в CDO и
// протекла бы в соседние тесты (тот же приём, что у гистерезиса в
// AmbientEntityTest.cpp).

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntitySpawner.h"
#include "Core/Entities/AmbientEntityActor.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "TestWorldHelpers.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    // Болотная клетка с Природой выше порога Трясинных духов: карточка без
    // временного условия, поэтому днём подходит только она.
    void MakeBogCell(FGridCell& Cell)
    {
        Cell.Biome = EBiomeType::Bog;
        Cell.bIsWater = false;
        Cell.bEternallyPure = false;
        Cell.Memory.bDegrading = false;
        for (FRealState* State : { &Cell.State, &Cell.TargetState })
        {
            State->Direction.Body = 0.1f;
            State->Direction.Mind = 0.1f;
            State->Direction.Spirit = 0.1f;
            State->Direction.Nature = 0.7f;
            State->Meta.Distortion = 0.2f;   // ниже порога Болотных огней
            State->Meta.Stability = 0.9f;    // выше порога Ржавых духов
            State->Meta.Corruption = 0.1f;   // ниже порога Гнильников
            State->Meta.Purity = 0.5f;
        }
    }

    // Весь мир -- одно болото: спавнер садится на любую клетку, и тест не
    // зависит от того, какой биом досталася процедурной генерации сетки.
    void MakeWholeGridBog(AGridWorldManager* Manager)
    {
        Manager->ForEachCell([](FGridCell& Cell) { MakeBogCell(Cell); });
    }

    int32 CountIndividuals(const AGridWorldManager* Manager)
    {
        int32 Count = 0;
        for (const auto& Pair : Manager->GetAmbientSpawners())
        {
            Count += Pair.Value.Individuals.Num();
        }
        return Count;
    }

    // Даёт спавнерам время выпустить всю стайку: особи выходят по одной с
    // интервалом AmbientSpawnIntervalSeconds.
    void RunSpawners(AGridWorldManager* Manager, int32 Steps, float Step = 5.0f)
    {
        for (int32 i = 0; i < Steps; ++i)
        {
            Manager->UpdateAmbientSpawners(Step);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawner_AutoSpawnersReleaseWanderingIndividuals,
    "Herbalist.Spawner.AutoSpawnersReleaseWanderingIndividuals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawner_AutoSpawnersReleaseWanderingIndividuals::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const bool bSaved = Settings->bUseAmbientSpawners;
    const float SavedSpacing = Settings->AmbientSpawnerSpacingMeters;
    const float SavedRadius = Settings->AmbientSpawnerRadiusMeters;
    Settings->bUseAmbientSpawners = true;
    // Тестовый мир -- 20x20 клеток по метру. Боевые 60 м не поместились бы в
    // него ни одним квадратом, поэтому шаг и радиус здесь в метрах мира, не
    // в боевых: проверяется механизм, не баланс.
    Settings->AmbientSpawnerSpacingMeters = 5.0f;
    Settings->AmbientSpawnerRadiusMeters = 2.0f;

    MakeWholeGridBog(Manager);
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // день: временных карточек нет
    RunSpawners(Manager, 3);

    const int32 SpawnerCount = Manager->GetAmbientSpawners().Num();
    TestTrue(FString::Printf(TEXT("Автоматические спавнеры расставлены по квадратам (%d)"), SpawnerCount), SpawnerCount > 0);
    // 20x20 клеток, квадрат 5 клеток -- 16 спавнеров вместо 400 существ:
    // ровно то, ради чего затевалось (раньше существо заводила КАЖДАЯ
    // подходящая клетка).
    TestTrue(FString::Printf(TEXT("Спавнеров много меньше, чем клеток (%d на 400)"), SpawnerCount), SpawnerCount <= 16);

    const FAmbientSpawnerRuntime* Any = nullptr;
    for (const auto& Pair : Manager->GetAmbientSpawners())
    {
        Any = &Pair.Value;
        break;
    }
    if (!TestNotNull(TEXT("Есть хотя бы один спавнер"), Any))
    {
        Settings->bUseAmbientSpawners = bSaved;
        Settings->AmbientSpawnerSpacingMeters = SavedSpacing;
        Settings->AmbientSpawnerRadiusMeters = SavedRadius;
        Manager->Destroy();
        return false;
    }
    TestEqual(TEXT("Днём на болоте спавнер держит Трясинных духов"), Any->ActiveEntityID, FName(TEXT("Трясинные духи")));
    TestTrue(TEXT("Спавнер выпустил особь"), Any->Individuals.Num() > 0);

    AAmbientEntityActor* Individual = Any->Individuals[0].Get();
    if (TestNotNull(TEXT("Особь жива"), Individual))
    {
        const FVector Center = Manager->GetCellWorldPosition(Any->CenterCell.X, Any->CenterCell.Y);
        TestTrue(TEXT("Особь появилась внутри зоны спавнера"),
            FVector2D(Individual->GetActorLocation() - Center).Size() <= Any->RadiusCm + 1.0f);

        // Брожение: за несколько секунд особь сдвигается, но из зоны не выходит.
        const FVector Before = Individual->GetActorLocation();
        for (int32 i = 0; i < 60; ++i)
        {
            Individual->Tick(0.1f);
        }
        const FVector After = Individual->GetActorLocation();
        TestTrue(TEXT("Особь бродит, а не стоит"), !After.Equals(Before, 1.0f));
        TestTrue(TEXT("Особь не выходит за радиус зоны"),
            FVector2D(After - Center).Size() <= Any->RadiusCm + 1.0f);
    }

    Settings->bUseAmbientSpawners = bSaved;
    Settings->AmbientSpawnerSpacingMeters = SavedSpacing;
    Settings->AmbientSpawnerRadiusMeters = SavedRadius;
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawner_ManualSpawnerOverridesAutoAndPicksItsOwnSpecies,
    "Herbalist.Spawner.ManualSpawnerOverridesAutoAndPicksItsOwnSpecies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawner_ManualSpawnerOverridesAutoAndPicksItsOwnSpecies::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const bool bSaved = Settings->bUseAmbientSpawners;
    Settings->bUseAmbientSpawners = true;

    MakeWholeGridBog(Manager);
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    // Ручной спавнер на клетке (10,10), радиус заведомо больше шага квадратов
    // -- ни один автоматический кандидат рядом не выживет.
    const FVector Center = Manager->GetCellWorldPosition(10, 10);
    AAmbientEntitySpawner* Manual = World->SpawnActor<AAmbientEntitySpawner>(AAmbientEntitySpawner::StaticClass(), Center, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Ручной спавнер создан"), Manual))
    {
        Settings->bUseAmbientSpawners = bSaved;
        Manager->Destroy();
        return false;
    }
    Manual->RadiusMeters = 1000.0f;
    Manual->bOverridesAuto = true;
    Manual->AllowedEntityIDs = { FName(TEXT("Гнильники")) };
    Manual->MaxIndividuals = 1;
    Manager->RegisterAmbientSpawner(Manual);

    // Гнильники просят Порчу выше 0.6 -- поднимаем её по всей сетке, иначе
    // список видов ручного спавнера оказался бы пуст.
    Manager->ForEachCell([](FGridCell& Cell)
    {
        Cell.State.Meta.Corruption = 0.9f;
        Cell.TargetState.Meta.Corruption = 0.9f;
    });

    RunSpawners(Manager, 3);

    TestEqual(TEXT("Ручной спавнер отменил автоматические в своём радиусе"), Manager->GetAmbientSpawners().Num(), 1);
    for (const auto& Pair : Manager->GetAmbientSpawners())
    {
        AddInfo(FString::Printf(TEXT("Спавнер на клетке (%d,%d), ручной=%d"), Pair.Key.X, Pair.Key.Y, Pair.Value.ManualSpawner.IsValid() ? 1 : 0));
    }
    const FAmbientSpawnerRuntime* Runtime = Manager->GetAmbientSpawners().Find(FIntPoint(10, 10));
    if (TestNotNull(TEXT("Спавнер стоит на своей клетке"), Runtime))
    {
        TestEqual(TEXT("Спавнер выпускает именно свой вид"), Runtime->ActiveEntityID, FName(TEXT("Гнильники")));
        TestEqual(TEXT("Потолок ручного спавнера соблюдён"), Runtime->Individuals.Num(), 1);
    }

    Manager->UnregisterAmbientSpawner(Manual);
    Manual->Destroy();
    Settings->bUseAmbientSpawners = bSaved;
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawner_CellPathStaysSilentWhileSpawnersOwnLowRank,
    "Herbalist.Spawner.CellPathStaysSilentWhileSpawnersOwnLowRank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawner_CellPathStaysSilentWhileSpawnersOwnLowRank::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const bool bSaved = Settings->bUseAmbientSpawners;

    FGridCell* Cell = Manager->GetCell(3, 3);
    if (!TestNotNull(TEXT("Клетка (3,3) есть"), Cell))
    {
        Manager->Destroy();
        return false;
    }
    MakeBogCell(*Cell);
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    // Сначала как раньше: клетка сама заводит Низшего.
    Settings->bUseAmbientSpawners = false;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Без спавнеров Низшего ставит клетка"), Cell->ManifestedEntityID, FName(TEXT("Трясинные духи")));

    // Со спавнерами клеточный путь отпускает Низшего и больше его не ставит.
    Settings->bUseAmbientSpawners = true;
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("Со спавнерами клетка Низшего не держит"), Cell->ManifestedEntityID.IsNone());
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("И не заводит заново"), Cell->ManifestedEntityID.IsNone());

    Settings->bUseAmbientSpawners = bSaved;
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
