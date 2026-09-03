// Source/ProjectHerbalistTests/Private/Tests/PcgResourcePlacementTest.cpp
//
// Расстановка ресурсов PCG-графом вместо клеточного C++-цикла (2026-09-03,
// подготовка к большому миру: "хочу не 1 ресурс на клетку, а произвольное
// количество, случайный порядок"). Три механизма, каждый закрывает свою
// дыру, которая иначе всплыла бы только в редакторе:
//
//   1. Актор, поставленный без Init() (PCG/рука), сам добирает данные строки
//      по IngredientID -- иначе он регистрировался бы на клетке с нулевым
//      BaseState, и сбор давал бы пустышку вместо травы.
//   2. Регион с bSpawnResourcesFromGrid=false C++ не заселяет -- иначе к
//      разбросу графа добавился бы второй набор внахлёст.
//   3. Отрастание собранного воспроизводит класс из строки, а не жёстко
//      базовый -- иначе Blueprint растения терялся бы после первого сбора.

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    const FName TestIngredientID(TEXT("PcgTestHerb"));

    // Отдельный реестр с одной узнаваемой строкой. Тот же приём, что уже в
    // IngredientRegistryAquaticTest.cpp: в голом editor-мире GameInstance
    // нет, и подсистему нельзя достать через мир -- строим свою и отдаём
    // актору явно.
    UIngredientRegistrySubsystem* MakeTestRegistry(float Resilience = 0.42f)
    {
        UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(OwnerGameInstance);

        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();

        // ID ингредиента -- это КЛЮЧ строки таблицы, отдельного поля в
        // FIngredientTableRow под него нет.
        FIngredientTableRow Row;
        Row.DisplayName = FText::FromString(TEXT("Тестовая трава"));
        Row.Resilience = Resilience;
        Row.bIronAverse = true;
        Row.BaseState.Meta.Purity = 0.77f;
        Row.AllowedBiomes.Add(EBiomeType::Taiga);
        Table->AddRow(TestIngredientID, Row);

        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    // Выставляет IngredientID ровно тем же способом, каким это делает
    // PCG-граф (override-пин узла Spawn Actor) или Blueprint: через
    // рефлексию. Само поле помечено BlueprintReadWrite, но в C++ лежит в
    // protected-секции, поэтому напрямую из теста недоступно -- и это
    // правильно: тест идёт тем же путём, что и настоящая расстановка.
    void SetIngredientIDLikePcgWould(AHerbalistResourceActor* Actor, FName IngredientID)
    {
        if (!Actor) return;
        if (FNameProperty* Prop = FindFProperty<FNameProperty>(AHerbalistResourceActor::StaticClass(), TEXT("IngredientID")))
        {
            Prop->SetPropertyValue_InContainer(Actor, IngredientID);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgResource_ActorWithoutInitResolvesRowFromRegistry,
    "Herbalist.PcgResource.ActorWithoutInitResolvesRowFromRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgResource_ActorWithoutInitResolvesRowFromRegistry::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UIngredientRegistrySubsystem* Registry = MakeTestRegistry();
    if (!TestNotNull(TEXT("Test registry built"), Registry)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Ровно то, что сделает PCG-граф: заспавнить актор и выставить один
    // IngredientID. Ни Init(), ни SetWorldManager, ни координат клетки.
    const FVector Pos = Manager->GetCellWorldPosition(3, 3);
    AHerbalistResourceActor* Actor = World->SpawnActor<AHerbalistResourceActor>(
        AHerbalistResourceActor::StaticClass(), Pos, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Resource actor spawned"), Actor)) { Manager->Destroy(); return false; }
    SetIngredientIDLikePcgWould(Actor, TestIngredientID);

    // BeginPlay сам определяет клетку и регистрируется; данные строки он тоже
    // пытается добрать, но в тестовом мире реестр недостижим (GameInstance
    // нет) -- поэтому отдаём его явно, тем же вызовом, что делает BeginPlay.
    Actor->DispatchBeginPlay();
    Actor->ResolveFromIngredientRegistry(Registry);

    TestEqual(TEXT("Resilience pulled from the row"), Actor->GetResilience(), 0.42f);
    TestTrue(TEXT("bIronAverse pulled from the row"), Actor->GetIsIronAverse());
    TestEqual(TEXT("BaseState pulled from the row"), Actor->GetBaseState().Meta.Purity, 0.77f);

    // И при этом он всё так же зарегистрирован на своей клетке.
    if (const FGridCell* Cell = Manager->GetCell(3, 3))
    {
        TestTrue(TEXT("Registered on its own cell"), Cell->ResourceActors.Contains(Actor));
    }

    Actor->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgResource_InitStillWinsOverRegistry,
    "Herbalist.PcgResource.InitStillWinsOverRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgResource_InitStillWinsOverRegistry::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UIngredientRegistrySubsystem* Registry = MakeTestRegistry();
    if (!TestNotNull(TEXT("Test registry built"), Registry)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FVector Pos = Manager->GetCellWorldPosition(4, 4);
    AHerbalistResourceActor* Actor = World->SpawnActor<AHerbalistResourceActor>(
        AHerbalistResourceActor::StaticClass(), Pos, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Resource actor spawned"), Actor)) { Manager->Destroy(); return false; }

    // C++-путь передаёт СВОИ значения, отличные от строки -- дозаправка из
    // реестра не должна их перетереть (Init идемпотентен по отношению к ней).
    FRealState Custom;
    Custom.Meta.Purity = 0.11f;
    Actor->Init(TestIngredientID, FText::FromString(TEXT("Своё имя")), nullptr, Custom, Pos, Manager, 4, 4,
        /*Resilience=*/0.99f, /*IronAverse=*/false, /*Delicate=*/false);
    Actor->DispatchBeginPlay();
    // Даже прямой вызов дозаправки после Init ничего не меняет.
    Actor->ResolveFromIngredientRegistry(Registry);

    TestEqual(TEXT("Init's Resilience survives BeginPlay"), Actor->GetResilience(), 0.99f);
    TestFalse(TEXT("Init's bIronAverse survives BeginPlay"), Actor->GetIsIronAverse());
    TestEqual(TEXT("Init's BaseState survives BeginPlay"), Actor->GetBaseState().Meta.Purity, 0.11f);

    Actor->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgResource_RegionOwnedByPcgIsNotPopulatedByCode,
    "Herbalist.PcgResource.RegionOwnedByPcgIsNotPopulatedByCode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgResource_RegionOwnedByPcgIsNotPopulatedByCode::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    MakeTestRegistry();

    // Регион на весь левый нижний угол сетки, отданный PCG.
    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Taiga, -50.0f, -50.0f, 450.0f, 450.0f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;
    Region->bSpawnResourcesFromGrid = false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) { Region->Destroy(); return false; }

    // Ни одна клетка, заявленная этим регионом, не должна получить ресурсов
    // от C++ -- их поставит граф, а не InitializeCells.
    int32 ClaimedCells = 0, CellsWithResources = 0;
    for (int32 Y = 0; Y < Manager->GridSizeY; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            const FGridCell* Cell = Manager->GetCell(X, Y);
            if (!Cell || Manager->GetClaimingRegion(*Cell) != Region) continue;
            ++ClaimedCells;
            if (Cell->ResourceActors.Num() > 0) ++CellsWithResources;
        }
    }

    TestTrue(TEXT("Sanity: the region actually claims some cells"), ClaimedCells > 0);
    TestEqual(TEXT("PCG-owned region gets no resources from the C++ cell loop"), CellsWithResources, 0);

    Manager->Destroy();
    Region->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
