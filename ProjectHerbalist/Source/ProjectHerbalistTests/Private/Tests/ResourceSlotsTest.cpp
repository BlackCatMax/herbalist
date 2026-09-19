// Source/ProjectHerbalistTests/Private/Tests/ResourceSlotsTest.cpp
//
// Слоты ресурсов (2026-09-19, этап 5б docs/research/DESIGN_Living_Vegetation_Research.md
// §4.1): PCG решает, где может стоять растение, C++ -- что там растёт. Ассет
// слотов; слот места не зависит от состояния клетки, разные места -- разные
// слоты; вид места решает пул видов (вода и кромка -- водные, суша -- без
// водных); водный вид разбросом на сушу не встаёт, без свободного водного
// слота или без водных видов -- земной вместо него; клетка без слотов --
// прежний разброс.

#include "Core/World/GridWorldManager.h"
#include "Core/World/ResourceSlots.h"
#include "Core/PCG/PCGHerbalistWriteResourceSlots.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Commandlets/PcgResourceSlotsSetupCommandlet.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    UHerbalistResourceSlots* MakeSlotsForTest(const TArray<FHerbalistResourceSlot>& Slots)
    {
        UHerbalistResourceSlots* Asset = NewObject<UHerbalistResourceSlots>(GetTransientPackage());
        TArray<FHerbalistResourceSlot> Copy = Slots;
        Asset->ReplaceSet(TEXT("Test"), MoveTemp(Copy));
        return Asset;
    }

    FHerbalistResourceSlot MakeSlotForTest(const FVector& Location, EResourceSlotKind Kind)
    {
        FHerbalistResourceSlot Slot;
        Slot.Location = Location;
        Slot.Kind = Kind;
        return Slot;
    }

    UIngredientRegistrySubsystem* MakeSlotsTestRegistry(bool bWithAquatic = true)
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();

        FIngredientTableRow Land;
        Land.AllowedBiomes = { EBiomeType::Bog };
        Table->AddRow(FName(TEXT("SlotsTestLand")), Land);

        if (bWithAquatic)
        {
            FIngredientTableRow Aquatic;
            Aquatic.AllowedBiomes = { EBiomeType::Bog };
            Aquatic.bGrowsOnWater = true;
            Table->AddRow(FName(TEXT("SlotsTestAquatic")), Aquatic);
        }

        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(NewObject<UGameInstance>(GEngine));
        Registry->LoadFromDataTable(Table);
        return Registry;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceSlots_AssetKindsAndPaths,
    "Herbalist.ResourceSlots.AssetKindsAndPaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceSlots_AssetKindsAndPaths::RunTest(const FString& Parameters)
{
    using namespace HerbalistResourceSlots;
    TestEqual(TEXT("Water"), ParseKind(TEXT("water")), EResourceSlotKind::Water);
    TestEqual(TEXT("Shore"), ParseKind(TEXT("Shore")), EResourceSlotKind::Shore);
    TestEqual(TEXT("Прочее -- Land"), ParseKind(TEXT("что угодно")), EResourceSlotKind::Land);

    TestTrue(TEXT("Водному -- вода"), SlotSuitsSpecies(EResourceSlotKind::Water, true));
    TestTrue(TEXT("Водному -- кромка"), SlotSuitsSpecies(EResourceSlotKind::Shore, true));
    TestFalse(TEXT("Водному -- не суша"), SlotSuitsSpecies(EResourceSlotKind::Land, true));
    TestTrue(TEXT("Земному -- суша"), SlotSuitsSpecies(EResourceSlotKind::Land, false));
    TestTrue(TEXT("Земному -- кромка (она на ландшафте)"), SlotSuitsSpecies(EResourceSlotKind::Shore, false));
    TestFalse(TEXT("Земному -- не вода"), SlotSuitsSpecies(EResourceSlotKind::Water, false));

    TestEqual(TEXT("Путь ассета карты"), AssetPackagePathForMap(TEXT("/Game/Maps/L_TestDev")), FString(TEXT("/Game/Data/ResourceSlots/RS_L_TestDev")));
    TestEqual(TEXT("Путь без префикса PIE"), AssetPackagePathForMap(TEXT("/Game/Maps/UEDPIE_0_L_TestDev")), FString(TEXT("/Game/Data/ResourceSlots/RS_L_TestDev")));

    // Перезапекание источника заменяет только его набор; пустой -- убирает.
    UHerbalistResourceSlots* Asset = NewObject<UHerbalistResourceSlots>(GetTransientPackage());
    Asset->ReplaceSet(TEXT("B"), { MakeSlotForTest(FVector(1, 0, 0), EResourceSlotKind::Land) });
    Asset->ReplaceSet(TEXT("A"), { MakeSlotForTest(FVector(2, 0, 0), EResourceSlotKind::Water), MakeSlotForTest(FVector(3, 0, 0), EResourceSlotKind::Shore) });
    TestEqual(TEXT("Два набора, три слота"), Asset->CountSlots(), 3);
    TestEqual(TEXT("Наборы по ключу -- порядок запекания не важен"), Asset->Sets[0].SourceKey, FString(TEXT("A")));
    Asset->ReplaceSet(TEXT("A"), { MakeSlotForTest(FVector(4, 0, 0), EResourceSlotKind::Land) });
    TestEqual(TEXT("Замена набора A"), Asset->CountSlots(), 2);
    Asset->ReplaceSet(TEXT("B"), {});
    TestEqual(TEXT("Пустой набор убирает источник"), Asset->Sets.Num(), 1);

    // Узел записи в редакторном мире без ассета на диске -- не создаёт его сам по запросу «не создавать».
    TestNull(TEXT("Нет ассета, создавать не просили -- null"),
        FPCGHerbalistWriteResourceSlotsElement::FindOrCreateSlotsAsset(TEXT("/Game/Maps/NoSuchMapForSlotsTest"), false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceSlots_AssignedSlotIsStableAndFollowsKind,
    "Herbalist.ResourceSlots.AssignedSlotIsStableAndFollowsKind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceSlots_AssignedSlotIsStableAndFollowsKind::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Слоты -- в километре над уровнем: проверка занятости не должна
    // зависеть от статики, стоящей в редакторном мире у начала координат.
    const FVector Corner = Manager->GetCellWorldPositionFlat(5, 5) + FVector(0.0f, 0.0f, 100000.0f);
    const float Quarter = Manager->CellSize * 0.25f;
    const FVector WaterPoint = Corner + FVector(Quarter, Quarter, 0.0f);
    const FVector ShorePoint = Corner + FVector(3.0f * Quarter, Quarter, 0.0f);
    const FVector LandPoint = Corner + FVector(Quarter, 3.0f * Quarter, 0.0f);
    Manager->SetResourceSlots(MakeSlotsForTest({ MakeSlotForTest(WaterPoint, EResourceSlotKind::Water),
        MakeSlotForTest(ShorePoint, EResourceSlotKind::Shore), MakeSlotForTest(LandPoint, EResourceSlotKind::Land) }));

    TestTrue(TEXT("У клетки (5,5) слоты есть"), Manager->HasResourceSlots(5, 5));
    TestFalse(TEXT("У клетки (6,6) слотов нет"), Manager->HasResourceSlots(6, 6));

    // Места 0..2 -- три разных слота, повтор -- те же, место 3 -- по кругу.
    TSet<FVector> Assigned;
    FHerbalistResourceSlot First;
    for (int32 Place = 0; Place < 3; ++Place)
    {
        FHerbalistResourceSlot Slot, Again;
        TestTrue(*FString::Printf(TEXT("Место %d: слот есть"), Place), Manager->GetAssignedResourceSlot(5, 5, Place, Slot));
        Manager->GetAssignedResourceSlot(5, 5, Place, Again);
        TestEqual(*FString::Printf(TEXT("Место %d: тот же слот при повторе"), Place), Again.Location, Slot.Location);
        Assigned.Add(Slot.Location);
        if (Place == 0) First = Slot;
    }
    TestEqual(TEXT("Три места -- три разных слота"), Assigned.Num(), 3);
    FHerbalistResourceSlot Wrapped;
    Manager->GetAssignedResourceSlot(5, 5, 3, Wrapped);
    TestEqual(TEXT("Место 3 -- по кругу слот места 0"), Wrapped.Location, First.Location);

    // Точка для вида: водному -- вода или кромка, земному -- суша или кромка.
    for (int32 Place = 0; Place < 3; ++Place)
    {
        FVector Aquatic, Land;
        TestTrue(TEXT("Водному место есть"), Manager->FindResourceSlotPosition(5, 5, Place, true, Aquatic));
        TestTrue(TEXT("Водному -- не суша"), Aquatic != LandPoint);
        TestTrue(TEXT("Земному место есть"), Manager->FindResourceSlotPosition(5, 5, Place, false, Land));
        TestTrue(TEXT("Земному -- не вода"), Land != WaterPoint);
    }

    // Клетка без слотов -- прежний поиск, не слот.
    FRandomStream FallbackRng(3);
    FVector Fallback;
    bool bFromSlot = true;
    Manager->FindResourcePosition(6, 6, 0, false, FallbackRng, Fallback, bFromSlot);
    TestFalse(TEXT("Без слотов -- прежний разброс"), bFromSlot);

    // Клетка только с сушей: водному виду места нет, разбросом он не ставится.
    Manager->SetResourceSlots(MakeSlotsForTest({ MakeSlotForTest(LandPoint, EResourceSlotKind::Land) }));
    FRandomStream AquaticRng(4);
    FVector AquaticFallback;
    TestFalse(TEXT("Водный вид без водного слота -- не ставится"),
        Manager->FindResourcePosition(5, 5, 0, true, AquaticRng, AquaticFallback, bFromSlot));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceSlots_WaterSlotGrowsAquaticSpeciesOnIt,
    "Herbalist.ResourceSlots.WaterSlotGrowsAquaticSpeciesOnIt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceSlots_WaterSlotGrowsAquaticSpeciesOnIt::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    UIngredientRegistrySubsystem* Registry = MakeSlotsTestRegistry();

    // Клетка суши, у которой граф нашёл только воду (плоскость воды заходит в
    // клетку): растёт водный вид -- ровно в точке слота, без сдвига региона.
    FGridCell* Cell = Manager->GetCell(5, 5);
    if (!TestNotNull(TEXT("Cell (5,5)"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Bog;
    Cell->bIsWater = false;
    Cell->BiomeWeights.Reset();
    const FVector WaterPoint = Manager->GetCellWorldPositionFlat(5, 5) + FVector(Manager->CellSize * 0.5f, Manager->CellSize * 0.5f, 100000.0f);
    Manager->SetResourceSlots(MakeSlotsForTest({ MakeSlotForTest(WaterPoint, EResourceSlotKind::Water) }));

    if (!Manager->IsCellMaterialized(*Cell))
    {
        AddInfo(TEXT("Клетка теста не материализована -- проверка места пропущена"));
        Manager->Destroy();
        return true;
    }

    FRandomStream SpeciesRng(5);
    const bool bSpawned = Manager->SpawnOneResourceInCell(*Cell, FHarvestContext(), nullptr, nullptr, Registry, SpeciesRng, 0);
    TestTrue(TEXT("Ресурс поставлен"), bSpawned);
    const AHerbalistResourceActor* Actor = Cell->ResourceActors.Num() > 0 ? Cell->ResourceActors.Last().Get() : nullptr;
    if (TestNotNull(TEXT("Актор ресурса"), Actor))
    {
        TestEqual(TEXT("Вид -- водный"), Actor->GetIngredientID(), FName(TEXT("SlotsTestAquatic")));
        TestTrue(*FString::Printf(TEXT("Стоит на слоте воды (%s)"), *Actor->GetActorLocation().ToString()),
            FVector::Dist2D(Actor->GetActorLocation(), WaterPoint) < 1.0f);
    }

    // Место 1 -- по кругу тот же слот воды, он занят: водному виду места нет,
    // вместо него земной (иначе клетка у воды навсегда теряла бы ресурс), и
    // он не на слоте воды.
    FRandomStream SecondRng(6);
    Manager->SpawnOneResourceInCell(*Cell, FHarvestContext(), nullptr, nullptr, Registry, SecondRng, 1);
    int32 OnWaterSlot = 0;
    for (const TWeakObjectPtr<AHerbalistResourceActor>& Spawned : Cell->ResourceActors)
    {
        if (!Spawned.IsValid())
        {
            continue;
        }
        OnWaterSlot += FVector::Dist2D(Spawned->GetActorLocation(), WaterPoint) < 1.0f ? 1 : 0;
        if (Spawned.Get() != Actor)
        {
            TestEqual(TEXT("Вместо водного без места -- земной"), Spawned->GetIngredientID(), FName(TEXT("SlotsTestLand")));
        }
    }
    TestEqual(TEXT("На слоте воды -- один ресурс"), OnWaterSlot, 1);

    // Слот суши -- только земной вид, хотя водный тоже в биоме.
    FGridCell* LandCell = Manager->GetCell(7, 7);
    if (TestNotNull(TEXT("Cell (7,7)"), LandCell) && Manager->IsCellMaterialized(*LandCell))
    {
        LandCell->Biome = EBiomeType::Bog;
        LandCell->bIsWater = false;
        LandCell->BiomeWeights.Reset();
        TArray<FHerbalistResourceSlot> LandSlots;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            LandSlots.Add(MakeSlotForTest(Manager->GetCellWorldPositionFlat(7, 7)
                + FVector(Manager->CellSize * (0.2f + 0.2f * Index), Manager->CellSize * 0.5f, 100000.0f), EResourceSlotKind::Land));
        }
        Manager->SetResourceSlots(MakeSlotsForTest(LandSlots));
        for (int32 Place = 0; Place < 4; ++Place)
        {
            FRandomStream LandRng(100 + Place);
            Manager->SpawnOneResourceInCell(*LandCell, FHarvestContext(), nullptr, nullptr, Registry, LandRng, Place);
        }
        TestEqual(TEXT("Четыре места -- четыре ресурса"), LandCell->ResourceActors.Num(), 4);
        for (const TWeakObjectPtr<AHerbalistResourceActor>& Spawned : LandCell->ResourceActors)
        {
            if (Spawned.IsValid())
            {
                TestEqual(TEXT("На суше -- земной вид"), Spawned->GetIngredientID(), FName(TEXT("SlotsTestLand")));
                Spawned->Destroy();
            }
        }
        LandCell->ResourceActors.Reset();

        // Водных видов в биоме нет: кромка даёт земной вид, ровно на слоте.
        const FVector ShorePoint = Manager->GetCellWorldPositionFlat(7, 7) + FVector(Manager->CellSize * 0.5f, Manager->CellSize * 0.5f, 100000.0f);
        Manager->SetResourceSlots(MakeSlotsForTest({ MakeSlotForTest(ShorePoint, EResourceSlotKind::Shore) }));
        FRandomStream ShoreRng(9);
        TestTrue(TEXT("Кромка без водных видов -- ресурс есть"),
            Manager->SpawnOneResourceInCell(*LandCell, FHarvestContext(), nullptr, nullptr, MakeSlotsTestRegistry(/*bWithAquatic=*/false), ShoreRng, 0));
        for (const TWeakObjectPtr<AHerbalistResourceActor>& Spawned : LandCell->ResourceActors)
        {
            if (Spawned.IsValid())
            {
                TestEqual(TEXT("Кромка без водных -- земной вид"), Spawned->GetIngredientID(), FName(TEXT("SlotsTestLand")));
                TestTrue(TEXT("Земной -- на слоте кромки"), FVector::Dist2D(Spawned->GetActorLocation(), ShorePoint) < 1.0f);
                Spawned->Destroy();
            }
        }
        LandCell->ResourceActors.Reset();
    }
    else
    {
        AddError(TEXT("Клетка (7,7) не материализована -- проверка слотов суши не прошла"));
    }

    for (const TWeakObjectPtr<AHerbalistResourceActor>& Spawned : Cell->ResourceActors)
    {
        if (Spawned.IsValid()) Spawned->Destroy();
    }
    Cell->ResourceActors.Reset();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceSlots_RestoreAndRegrowthKeepSlots,
    "Herbalist.ResourceSlots.RestoreAndRegrowthKeepSlots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceSlots_RestoreAndRegrowthKeepSlots::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    UIngredientRegistrySubsystem* Registry = MakeSlotsTestRegistry();

    FGridCell* Cell = Manager->GetCell(5, 5);
    if (!TestNotNull(TEXT("Cell (5,5)"), Cell) || !Manager->IsCellMaterialized(*Cell))
    {
        AddError(TEXT("Клетка (5,5) не материализована -- проверка не прошла"));
        Manager->Destroy();
        return false;
    }
    Cell->Biome = EBiomeType::Bog;
    Cell->bIsWater = false;
    Cell->BiomeWeights.Reset();
    const FVector Corner = Manager->GetCellWorldPositionFlat(5, 5) + FVector(0.0f, 0.0f, 100000.0f);
    const float Quarter = Manager->CellSize * 0.25f;
    const TArray<FVector> Points = { Corner + FVector(Quarter, Quarter, 0.0f), Corner + FVector(3.0f * Quarter, Quarter, 0.0f),
        Corner + FVector(Quarter, 3.0f * Quarter, 0.0f) };
    Manager->SetResourceSlots(MakeSlotsForTest({ MakeSlotForTest(Points[0], EResourceSlotKind::Water),
        MakeSlotForTest(Points[1], EResourceSlotKind::Shore), MakeSlotForTest(Points[2], EResourceSlotKind::Land) }));

    auto ClearCell = [Cell]()
    {
        for (const TWeakObjectPtr<AHerbalistResourceActor>& Spawned : Cell->ResourceActors)
        {
            if (Spawned.IsValid())
            {
                Spawned->Destroy();
            }
        }
        Cell->ResourceActors.Reset();
    };
    auto FindAtPlace = [Cell](int32 Place) -> AHerbalistResourceActor*
    {
        for (const TWeakObjectPtr<AHerbalistResourceActor>& Spawned : Cell->ResourceActors)
        {
            if (Spawned.IsValid() && Spawned->GetPlacementSlot() == Place)
            {
                return Spawned.Get();
            }
        }
        return nullptr;
    };

    // Первичное заселение: три места -- три слота.
    TArray<FName> Species;
    TArray<FVector> Locations;
    for (int32 Place = 0; Place < 3; ++Place)
    {
        FRandomStream SpeciesRng(20 + Place);
        TestTrue(*FString::Printf(TEXT("Место %d заселено"), Place),
            Manager->SpawnOneResourceInCell(*Cell, FHarvestContext(), nullptr, nullptr, Registry, SpeciesRng, Place));
        const AHerbalistResourceActor* Grown = FindAtPlace(Place);
        if (!TestNotNull(*FString::Printf(TEXT("Актор места %d"), Place), Grown))
        {
            ClearCell();
            Manager->Destroy();
            return false;
        }
        Species.Add(Grown->GetIngredientID());
        Locations.Add(Grown->GetActorLocation());
    }

    // Сейв и пробуждение спящей клетки (SpawnResourceActor) в обратном порядке
    // -- другое состояние клетки, те же места.
    ClearCell();
    for (int32 Place = 2; Place >= 0; --Place)
    {
        Manager->SpawnResourceActor(Species[Place], 5, 5, FVector::ZeroVector, Registry, Place);
        const AHerbalistResourceActor* Restored = FindAtPlace(Place);
        if (TestNotNull(*FString::Printf(TEXT("Место %d восстановлено"), Place), Restored))
        {
            TestTrue(*FString::Printf(TEXT("Место %d -- на прежней точке"), Place),
                FVector::Dist2D(Restored->GetActorLocation(), Locations[Place]) < 1.0f);
        }
    }

    // Сбор места 1 и отрастание: новый индекс места по кругу попадает на
    // занятый слот -- ресурс всё равно вырастает, и не в занятую точку.
    if (AHerbalistResourceActor* Harvested = FindAtPlace(1))
    {
        Cell->ResourceActors.Remove(Harvested);
        Harvested->Destroy();
    }
    FRandomStream RegrowthRng(40);
    TestTrue(TEXT("Отрастание поставило ресурс"), Manager->SpawnOneResourceInCell(*Cell, FHarvestContext(), nullptr, nullptr, Registry,
        RegrowthRng, AGridWorldManager::AllocatePlacementSlot(*Cell)));
    TestEqual(TEXT("В клетке снова три ресурса"), Cell->ResourceActors.Num(), 3);
    for (const FVector& Point : Points)
    {
        int32 OnPoint = 0;
        for (const TWeakObjectPtr<AHerbalistResourceActor>& Spawned : Cell->ResourceActors)
        {
            OnPoint += Spawned.IsValid() && FVector::Dist2D(Spawned->GetActorLocation(), Point) < 1.0f ? 1 : 0;
        }
        TestTrue(TEXT("На слоте не больше одного ресурса"), OnPoint <= 1);
    }

    ClearCell();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceSlots_SlotsGraphAndWaterVolume,
    "Herbalist.ResourceSlots.SlotsGraphAndWaterVolume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceSlots_SlotsGraphAndWaterVolume::RunTest(const FString& Parameters)
{
    // Сборка на временном графе: все связи на месте (иначе -1), три вида
    // места, повтор ничего не меняет.
    UPCGGraph* Scratch = NewObject<UPCGGraph>(GetTransientPackage());
    TestEqual(TEXT("Граф собран"), UPcgResourceSlotsSetupCommandlet::BuildSlotsGraph(Scratch), 1);
    TestEqual(TEXT("Повтор -- уже собран"), UPcgResourceSlotsSetupCommandlet::BuildSlotsGraph(Scratch), 0);
    TSet<FString> Kinds;
    for (const UPCGNode* Node : Scratch->GetNodes())
    {
        const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
        if (!Settings || Settings->GetClass()->GetName() != TEXT("PCGAddAttributeSettings")) continue;
        const FStructProperty* Constant = CastField<FStructProperty>(Settings->GetClass()->FindPropertyByName(TEXT("AttributeTypes")));
        const FStrProperty* Value = Constant ? CastField<FStrProperty>(Constant->Struct->FindPropertyByName(TEXT("StringValue"))) : nullptr;
        if (Value) Kinds.Add(Value->GetPropertyValue_InContainer(Constant->ContainerPtrToValuePtr<void>(Settings)));
    }
    TestTrue(TEXT("Вода, кромка и суша"), Kinds.Contains(TEXT("Water")) && Kinds.Contains(TEXT("Shore")) && Kinds.Contains(TEXT("Land")));

    // Проект: граф сохранён -run=PcgResourceSlotsSetup, BP_WaterVolume несёт
    // компонент с ним, генерация по запросу -- в игре граф не работает.
    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, TEXT("/Game/PCG/PCG_ResourceSlots.PCG_ResourceSlots"));
    if (!TestNotNull(TEXT("PCG_ResourceSlots в проекте"), Graph)) return false;
    bool bHasWrite = false;
    for (const UPCGNode* Node : Graph->GetNodes())
    {
        bHasWrite |= Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGHerbalistWriteResourceSlotsSettings>();
    }
    TestTrue(TEXT("В графе есть запись слотов"), bHasWrite);

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/BP_WaterVolume.BP_WaterVolume"));
    if (!TestNotNull(TEXT("BP_WaterVolume"), Blueprint) || !Blueprint->SimpleConstructionScript) return false;
    const UPCGComponent* Template = nullptr;
    for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        const UPCGComponent* Candidate = Node ? Cast<UPCGComponent>(Node->ComponentTemplate) : nullptr;
        if (Candidate && Candidate->GetGraph() == Graph) Template = Candidate;
    }
    if (TestNotNull(TEXT("Компонент слотов в BP_WaterVolume"), Template))
    {
        TestEqual(TEXT("Генерация по запросу"), Template->GenerationTrigger, EPCGComponentGenerationTrigger::GenerateOnDemand);
    }
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
