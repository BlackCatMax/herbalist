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
#include "Core/Entities/ArtifactTypes.h"
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

// ---- Этап 2: эффект особи в радиусе, Гребень, подавление появления ----

namespace
{
    // Ручной спавнер Гнильников с одной особью в известной клетке: весь мир
    // -- болото с высокой Порчей, чтобы карточка подходила.
    AAmbientEntitySpawner* MakeManualGnilnikiSpawner(AGridWorldManager* Manager, UWorld* World, const FIntPoint& Cell)
    {
        Manager->ForEachCell([](FGridCell& C)
        {
            MakeBogCell(C);
            C.State.Meta.Corruption = 0.9f;
            C.TargetState.Meta.Corruption = 0.9f;
        });
        AAmbientEntitySpawner* Manual = World->SpawnActor<AAmbientEntitySpawner>(
            AAmbientEntitySpawner::StaticClass(), Manager->GetCellWorldPosition(Cell.X, Cell.Y), FRotator::ZeroRotator);
        if (!Manual) return nullptr;
        Manual->RadiusMeters = 1.0f;   // особь стоит почти в своей клетке
        Manual->bOverridesAuto = true;
        Manual->AllowedEntityIDs = { FName(TEXT("Гнильники")) };
        Manual->MaxIndividuals = 1;
        Manager->RegisterAmbientSpawner(Manual);
        return Manual;
    }

    int32 IndividualCountAt(const AGridWorldManager* Manager, const FIntPoint& Key)
    {
        const FAmbientSpawnerRuntime* Runtime = Manager->GetAmbientSpawners().Find(Key);
        return Runtime ? Runtime->Individuals.Num() : -1;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawner_IndividualEffectFallsOffWithDistance,
    "Herbalist.Spawner.IndividualEffectFallsOffWithDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawner_IndividualEffectFallsOffWithDistance::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const bool bSaved = Settings->bUseAmbientSpawners;
    const float SavedEffect = Settings->AmbientEffectRadiusMeters;
    Settings->bUseAmbientSpawners = true;
    Settings->AmbientEffectRadiusMeters = 5.0f;   // тестовый мир -- метровые клетки

    AAmbientEntitySpawner* Manual = MakeManualGnilnikiSpawner(Manager, World, FIntPoint(10, 10));
    if (!TestNotNull(TEXT("Ручной спавнер создан"), Manual))
    {
        Settings->bUseAmbientSpawners = bSaved;
        Settings->AmbientEffectRadiusMeters = SavedEffect;
        Manager->Destroy();
        return false;
    }
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    const FGridCell* Near = Manager->GetCellConst(10, 10);
    const FGridCell* Mid = Manager->GetCellConst(13, 10);
    const FGridCell* Far = Manager->GetCellConst(17, 10);
    const float PurityNearBefore = Near->TargetState.Meta.Purity;
    const float PurityMidBefore = Mid->TargetState.Meta.Purity;
    const float PurityFarBefore = Far->TargetState.Meta.Purity;

    RunSpawners(Manager, 3);

    // Гнильники: Порча вверх, Чистота вниз. Клетка под особью должна просесть
    // сильнее средней, дальняя (вне радиуса) -- не тронута вовсе.
    const float NearDrop = PurityNearBefore - Near->TargetState.Meta.Purity;
    const float MidDrop = PurityMidBefore - Mid->TargetState.Meta.Purity;
    const float FarDrop = PurityFarBefore - Far->TargetState.Meta.Purity;

    TestTrue(FString::Printf(TEXT("Клетка под особью просела (%.5f)"), NearDrop), NearDrop > 0.0f);
    TestTrue(FString::Printf(TEXT("Дальше эффект слабее (%.5f < %.5f)"), MidDrop, NearDrop), MidDrop < NearDrop);
    TestTrue(FString::Printf(TEXT("За радиусом эффекта нет (%.5f)"), FarDrop), FMath::IsNearlyZero(FarDrop));

    Manager->UnregisterAmbientSpawner(Manual);
    Manual->Destroy();
    Settings->bUseAmbientSpawners = bSaved;
    Settings->AmbientEffectRadiusMeters = SavedEffect;
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawner_CombDispelsIndividualAndHoldsRespawn,
    "Herbalist.Spawner.CombDispelsIndividualAndHoldsRespawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawner_CombDispelsIndividualAndHoldsRespawn::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const bool bSaved = Settings->bUseAmbientSpawners;
    Settings->bUseAmbientSpawners = true;

    AAmbientEntitySpawner* Manual = MakeManualGnilnikiSpawner(Manager, World, FIntPoint(10, 10));
    if (!TestNotNull(TEXT("Ручной спавнер создан"), Manual))
    {
        Settings->bUseAmbientSpawners = bSaved;
        Manager->Destroy();
        return false;
    }
    Manager->SetGameClockSeconds(10.0f * 60.0f);
    RunSpawners(Manager, 3);

    const FAmbientSpawnerRuntime* Runtime = Manager->GetAmbientSpawners().Find(FIntPoint(10, 10));
    if (!TestNotNull(TEXT("Спавнер на своей клетке"), Runtime)
        || !TestTrue(TEXT("Особь вышла"), Runtime->Individuals.Num() > 0)
        || !TestTrue(TEXT("Особь жива"), Runtime->Individuals[0].IsValid()))
    {
        Manager->UnregisterAmbientSpawner(Manual);
        Manual->Destroy();
        Settings->bUseAmbientSpawners = bSaved;
        Manager->Destroy();
        return false;
    }

    // Клетка, где реально стоит особь: зона метровая, но особь могла сойти
    // и на соседнюю.
    int32 IndividualX = 0, IndividualY = 0;
    Manager->WorldPositionToCell(Runtime->Individuals[0]->GetActorLocation(), IndividualX, IndividualY);

    TArray<FAcquiredArtifact> Artifacts = Manager->GetAcquiredArtifacts();
    FAcquiredArtifact Comb;
    Comb.ArtifactID = FName(TEXT("Гребень"));
    Artifacts.Add(Comb);
    Manager->SetAcquiredArtifacts(Artifacts);

    TestTrue(TEXT("Гребень сработал"), Manager->UseCombOnCell(FIntPoint(IndividualX, IndividualY)));
    TestEqual(TEXT("Особь погасла"), IndividualCountAt(Manager, FIntPoint(10, 10)), 0);

    // Пауза AmbientRespawnSeconds (60 с по умолчанию): за три такта по 5 с
    // новая особь не выходит.
    RunSpawners(Manager, 3);
    TestEqual(TEXT("Спавнер молчит после Гребня"), IndividualCountAt(Manager, FIntPoint(10, 10)), 0);

    // Когда пауза вышла -- выпускает снова.
    RunSpawners(Manager, 4, 30.0f);
    TestTrue(TEXT("После паузы особь вернулась"), IndividualCountAt(Manager, FIntPoint(10, 10)) > 0);

    Manager->UnregisterAmbientSpawner(Manual);
    Manual->Destroy();
    Settings->bUseAmbientSpawners = bSaved;
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawner_SilverWardBlocksNewIndividuals,
    "Herbalist.Spawner.SilverWardBlocksNewIndividuals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawner_SilverWardBlocksNewIndividuals::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const bool bSaved = Settings->bUseAmbientSpawners;
    Settings->bUseAmbientSpawners = true;

    AAmbientEntitySpawner* Manual = MakeManualGnilnikiSpawner(Manager, World, FIntPoint(10, 10));
    if (!TestNotNull(TEXT("Ручной спавнер создан"), Manual))
    {
        Settings->bUseAmbientSpawners = bSaved;
        Manager->Destroy();
        return false;
    }
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    // Серебряный оберег -- общий на всю сетку: новых особей не выпускает
    // никто, хотя вид спавнер держит.
    Manager->SetSilverWardActive(true);
    RunSpawners(Manager, 3);
    const FAmbientSpawnerRuntime* Runtime = Manager->GetAmbientSpawners().Find(FIntPoint(10, 10));
    if (TestNotNull(TEXT("Спавнер на своей клетке"), Runtime))
    {
        TestEqual(TEXT("Под оберегом вид выбран"), Runtime->ActiveEntityID, FName(TEXT("Гнильники")));
        TestEqual(TEXT("Под оберегом особей нет"), Runtime->Individuals.Num(), 0);
    }

    // Оберег сняли -- особь выходит.
    Manager->SetSilverWardActive(false);
    RunSpawners(Manager, 3);
    TestTrue(TEXT("Без оберега особь вышла"), IndividualCountAt(Manager, FIntPoint(10, 10)) > 0);

    Manager->UnregisterAmbientSpawner(Manual);
    Manual->Destroy();
    Settings->bUseAmbientSpawners = bSaved;
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawner_LocalSuppressionOnlyBlocksItsOwnSpot,
    "Herbalist.Spawner.LocalSuppressionOnlyBlocksItsOwnSpot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawner_LocalSuppressionOnlyBlocksItsOwnSpot::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const bool bSaved = Settings->bUseAmbientSpawners;
    Settings->bUseAmbientSpawners = true;

    AAmbientEntitySpawner* Manual = MakeManualGnilnikiSpawner(Manager, World, FIntPoint(10, 10));
    if (!TestNotNull(TEXT("Ручной спавнер создан"), Manual))
    {
        Settings->bUseAmbientSpawners = bSaved;
        Manager->Destroy();
        return false;
    }
    Manual->RadiusMeters = 4.0f;   // зона в несколько клеток: есть куда отойти
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    // Закрываем пером Жар-птицы почти всю зону, кроме центра и угла: это
    // местное подавление точки появления, как оберег или Шапка, только его
    // легко поставить в тесте. Спавнер обязан найти свободное место, а не
    // упереться в одну и ту же отклонённую точку (ревью 2026-09-20).
    for (int32 Y = 6; Y <= 14; ++Y)
    {
        for (int32 X = 6; X <= 14; ++X)
        {
            if (FGridCell* Cell = Manager->GetCell(X, Y))
            {
                // Клетка центра свободна всегда: по ней спавнер читает
                // условия карточки, и вечно чистый центр просто выключил бы
                // его целиком, ничего не проверив.
                const bool bFree = (X >= 11 && X <= 13 && Y >= 11 && Y <= 13) || (X == 10 && Y == 10);
                Cell->bEternallyPure = !bFree;
            }
        }
    }

    // Свободен только угол зоны -- случайная точка попадает в него не сразу,
    // и это ровно то, что проверяется: спавнер должен пробовать НОВЫЕ точки.
    RunSpawners(Manager, 60);

    const FAmbientSpawnerRuntime* Runtime = Manager->GetAmbientSpawners().Find(FIntPoint(10, 10));
    if (TestNotNull(TEXT("Спавнер на своей клетке"), Runtime))
    {
        TestTrue(FString::Printf(TEXT("Особь нашла свободное место за %d попыток"), Runtime->RejectedSpawnAttempts),
            Runtime->Individuals.Num() > 0);
        if (Runtime->Individuals.Num() > 0 && Runtime->Individuals[0].IsValid())
        {
            int32 CellX = 0, CellY = 0;
            Manager->WorldPositionToCell(Runtime->Individuals[0]->GetActorLocation(), CellX, CellY);
            const bool bOnFreeCell = (CellX >= 11 && CellX <= 13 && CellY >= 11 && CellY <= 13) || (CellX == 10 && CellY == 10);
            TestTrue(FString::Printf(TEXT("И встала на свободной клетке, а не под пером (%d,%d)"), CellX, CellY), bOnFreeCell);
        }
    }

    for (int32 Y = 6; Y <= 14; ++Y)
    {
        for (int32 X = 6; X <= 14; ++X)
        {
            if (FGridCell* Cell = Manager->GetCell(X, Y))
            {
                Cell->bEternallyPure = false;
            }
        }
    }
    Manager->UnregisterAmbientSpawner(Manual);
    Manual->Destroy();
    Settings->bUseAmbientSpawners = bSaved;
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
