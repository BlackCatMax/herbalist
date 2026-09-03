// Source/ProjectHerbalistTests/Private/Tests/ShrineActorTest.cpp
//
// Капище как отдельное место (2026-09-02, пересмотр решения v1). Раньше
// капище существовало только на клетке котла (AAlchemyTableActor::BeginPlay
// его регистрировал), теперь его ставит левел-дизайнер актором AShrineActor,
// а котёл капища не создаёт вовсе. Проверяется обе стороны развязки: что
// новый актор действительно регистрирует капище (с типом от клетки или
// заданным руками, со стартовым Restoration), и что котёл больше НЕ
// регистрирует — иначе развязка была бы только на словах.
//
// DispatchBeginPlay-паттерн — тот же, что в EntityActorTest.cpp: в голом
// editor-мире SpawnActor не вызывает BeginPlay сам.

#include "Core/World/GridWorldManager.h"
#include "Core/Shrine/ShrineActor.h"
#include "Core/Shrine/ShrineTypes.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Ставит актор ровно в центр клетки (X,Y) и прогоняет его BeginPlay.
    template <typename T>
    T* SpawnAtCell(UWorld* World, AGridWorldManager* Manager, int32 X, int32 Y)
    {
        const FVector Pos = Manager->GetCellWorldPosition(X, Y);
        T* Actor = World->SpawnActor<T>(T::StaticClass(), Pos, FRotator::ZeroRotator);
        if (Actor) Actor->DispatchBeginPlay();
        return Actor;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineActor_RegistersShrineOnItsCell,
    "Herbalist.ShrineActor.RegistersShrineOnItsCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineActor_RegistersShrineOnItsCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestEqual(TEXT("No shrines before the actor is placed"), Manager->GetShrines().Num(), 0);

    AShrineActor* Shrine = SpawnAtCell<AShrineActor>(World, Manager, 3, 3);
    if (!TestNotNull(TEXT("AShrineActor spawned"), Shrine)) { Manager->Destroy(); return false; }

    TestEqual(TEXT("Exactly one shrine registered"), Manager->GetShrines().Num(), 1);
    if (Manager->GetShrines().Num() == 1)
    {
        TestEqual(TEXT("Registered on the actor's own cell"), Manager->GetShrines()[0].Cell, FIntPoint(3, 3));
        TestEqual(TEXT("Restoration starts at the default 0"), Manager->GetShrines()[0].Restoration, 0.0f);
    }

    Shrine->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineActor_ExplicitTypeOverridesCellResolution,
    "Herbalist.ShrineActor.ExplicitTypeOverridesCellResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineActor_ExplicitTypeOverridesCellResolution::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Клетка и все её 4 соседа — Тайга: без границы биомов ResolveShrineTypeForCell
    // вернул бы Forest, значит явный Water ниже виден именно как переопределение.
    const FIntPoint Coord(5, 5);
    static const FIntPoint Offsets[5] = { FIntPoint(0,0), FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1) };
    for (const FIntPoint& Offset : Offsets)
    {
        if (FGridCell* C = Manager->GetCell(Coord.X + Offset.X, Coord.Y + Offset.Y))
        {
            C->Biome = EBiomeType::Taiga;
        }
    }
    TestEqual(TEXT("Sanity: cell resolves to Forest on its own"),
        (int32)Manager->ResolveShrineTypeForCell(Coord), (int32)EShrineType::Forest);

    const FVector Pos = Manager->GetCellWorldPosition(Coord.X, Coord.Y);
    AShrineActor* Shrine = World->SpawnActor<AShrineActor>(AShrineActor::StaticClass(), Pos, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("AShrineActor spawned"), Shrine)) { Manager->Destroy(); return false; }
    Shrine->bResolveTypeFromCell = false;
    Shrine->ShrineType = EShrineType::Water;
    Shrine->InitialRestoration = 0.8f;
    Shrine->DispatchBeginPlay();

    if (TestEqual(TEXT("One shrine registered"), Manager->GetShrines().Num(), 1))
    {
        TestEqual(TEXT("Explicit type wins over the cell's own biome group"),
            (int32)Manager->GetShrines()[0].Type, (int32)EShrineType::Water);
        TestEqual(TEXT("InitialRestoration applied on creation"),
            Manager->GetShrines()[0].Restoration, 0.8f);
    }

    Shrine->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineActor_SecondActorOnSameCellKeepsAccumulatedRestoration,
    "Herbalist.ShrineActor.SecondActorOnSameCellKeepsAccumulatedRestoration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineActor_SecondActorOnSameCellKeepsAccumulatedRestoration::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Капище уже накопило ухоженность (как после подношений).
    Manager->RegisterShrine(FIntPoint(2, 2), EShrineType::Ancestral, 0.6f);
    TestEqual(TEXT("Sanity: accumulated Restoration is in place"), Manager->GetShrines()[0].Restoration, 0.6f);

    // Второй актор на той же клетке не должен откатить её к своему стартовому
    // значению -- иначе перезапуск/дубль актора стирал бы прогресс игрока.
    const FVector Pos = Manager->GetCellWorldPosition(2, 2);
    AShrineActor* Shrine = World->SpawnActor<AShrineActor>(AShrineActor::StaticClass(), Pos, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("AShrineActor spawned"), Shrine)) { Manager->Destroy(); return false; }
    Shrine->InitialRestoration = 0.0f;
    Shrine->DispatchBeginPlay();

    TestEqual(TEXT("Still a single shrine, no duplicate"), Manager->GetShrines().Num(), 1);
    TestEqual(TEXT("Accumulated Restoration survives re-registration"),
        Manager->GetShrines()[0].Restoration, 0.6f);

    Shrine->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineActor_AlchemyTableNoLongerRegistersShrine,
    "Herbalist.ShrineActor.AlchemyTableNoLongerRegistersShrine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineActor_AlchemyTableNoLongerRegistersShrine::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    AAlchemyTableActor* Table = SpawnAtCell<AAlchemyTableActor>(World, Manager, 4, 4);
    if (!TestNotNull(TEXT("AAlchemyTableActor spawned"), Table)) { Manager->Destroy(); return false; }

    // Суть развязки 2026-09-02: место варки само по себе капищем больше не
    // является. Домовой при этом остаётся привязан к котлу -- он про очаг.
    TestEqual(TEXT("Cauldron registers no shrine any more"), Manager->GetShrines().Num(), 0);

    bool bFoundDomovoi = false;
    for (const FEntityLandmark& Landmark : Manager->GetEntityLandmarks())
    {
        if (Landmark.EntityID == FName(TEXT("Домовой"))) { bFoundDomovoi = true; break; }
    }
    TestTrue(TEXT("Домовой is still registered by the cauldron (hearth, not shrine)"), bFoundDomovoi);

    Table->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
