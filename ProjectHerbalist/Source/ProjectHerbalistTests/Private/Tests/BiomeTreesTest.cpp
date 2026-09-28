// Source/ProjectHerbalistTests/Private/Tests/BiomeTreesTest.cpp
//
// Деревья биомов (2026-09-28, -run=PcgTreesSetup): правила узла «Herbalist
// Biome Trees», сборка графа PCG_Trees тем же кодом, что у коммандлета, на
// временном графе, разбор biome_trees.json и состояние проекта после
// коммандлета.

#include "Commandlets/PcgTreesSetupCommandlet.h"
#include "Core/PCG/PCGHerbalistBiomeTrees.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/AutomationTest.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "UObject/UnrealType.h"
#include "TestWorldHelpers.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeTrees_KeepAndPickRules,
    "Herbalist.BiomeTrees.KeepAndPickRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeTrees_KeepAndPickRules::RunTest(const FString& Parameters)
{
    using S = UPCGHerbalistBiomeTreesSettings;
    TestEqual(TEXT("Доля -- плотность биома к плотности сэмплера"), S::KeepFraction(1.0f, 4.0f, 1.0f), 0.25f, 1e-5f);
    TestEqual(TEXT("К краю региона -- реже"), S::KeepFraction(2.0f, 4.0f, 0.5f), 0.25f, 1e-5f);
    TestEqual(TEXT("Больше сэмплера -- не больше 1"), S::KeepFraction(8.0f, 4.0f, 1.0f), 1.0f, 1e-5f);
    TestEqual(TEXT("Без деревьев -- ноль"), S::KeepFraction(0.0f, 4.0f, 1.0f), 0.0f, 1e-5f);

    FHerbalistBiomeTreeRow Pine;
    Pine.TreesPer100SquareMeters = 3.0f;
    FHerbalistBiomeTreeRow Bush;
    Bush.TreesPer100SquareMeters = 1.0f;
    FHerbalistBiomeTreeRow Empty;
    Empty.TreesPer100SquareMeters = 0.0f;
    const TArray<const FHerbalistBiomeTreeRow*> Rows = { &Pine, &Empty, &Bush };
    TestEqual(TEXT("Начало -- первая строка"), S::PickRow(Rows, 0.0f), 0);
    TestEqual(TEXT("Три четверти -- ещё первая (вес 3 из 4)"), S::PickRow(Rows, 0.74f), 0);
    TestEqual(TEXT("Дальше -- куст, пустая строка пропущена"), S::PickRow(Rows, 0.76f), 2);
    TestEqual(TEXT("Край -- последняя с весом"), S::PickRow(Rows, 1.0f), 2);
    TestEqual(TEXT("Нет веса -- нет строки"), S::PickRow({ &Empty }, 0.5f), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeTrees_NoTreesOverWater,
    "Herbalist.BiomeTrees.NoTreesOverWater",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeTrees_NoTreesOverWater::RunTest(const FString& Parameters)
{
    // 2026-09-28, по PIE: деревья в пруду. Над водой -- по X-Y, высота точки
    // не важна: сплайн пруда и точка на ландшафте на разной высоте.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world"), World)) return false;
    AWaterRegionVolume* Pond = SpawnWaterRegionCoveringWorldRect(World, 900000.0f, 900000.0f, 901000.0f, 901000.0f);
    if (!TestNotNull(TEXT("Пруд"), Pond)) return false;

    using S = UPCGHerbalistBiomeTreesSettings;
    const TArray<const AWaterRegionVolume*> Waters = { Pond };
    TestTrue(TEXT("Посреди пруда -- вода"), S::IsOverWater(Waters, FVector(900500.0f, 900500.0f, 0.0f)));
    TestTrue(TEXT("Высота не важна"), S::IsOverWater(Waters, FVector(900500.0f, 900500.0f, -700.0f)));
    TestFalse(TEXT("Рядом с прудом -- суша"), S::IsOverWater(Waters, FVector(901200.0f, 900500.0f, 0.0f)));
    TestFalse(TEXT("Воды нет -- суша"), S::IsOverWater({}, FVector(900500.0f, 900500.0f, 0.0f)));
    Pond->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeTrees_GraphBuildsOnce,
    "Herbalist.BiomeTrees.GraphBuildsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeTrees_GraphBuildsOnce::RunTest(const FString& Parameters)
{
    // Все связи проверяет сам сборщик (иначе -1); повтор ничего не меняет.
    UPCGGraph* Scratch = NewObject<UPCGGraph>(GetTransientPackage());
    TestEqual(TEXT("Граф собран"), UPcgTreesSetupCommandlet::BuildTreesGraph(Scratch), 1);
    TestEqual(TEXT("Повтор -- уже собран"), UPcgTreesSetupCommandlet::BuildTreesGraph(Scratch), 0);
    // Воду отбрасывает узел деревьев; вычитания поверхности пруда в графе нет
    // (2026-09-28: оно сравнивало точки в 3D и не срабатывало).
    for (const UPCGNode* Node : Scratch->GetNodes())
    {
        const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
        TestFalse(TEXT("Нет узла Difference"), Settings && Settings->GetClass()->GetName() == TEXT("PCGDifferenceSettings"));
    }

    const UPCGHerbalistBiomeTreesSettings* Trees = nullptr;
    const UPCGSettings* Spawner = nullptr;
    for (const UPCGNode* Node : Scratch->GetNodes())
    {
        const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
        if (const UPCGHerbalistBiomeTreesSettings* Found = Cast<UPCGHerbalistBiomeTreesSettings>(Settings)) Trees = Found;
        if (Settings && Settings->GetClass()->GetName() == TEXT("PCGStaticMeshSpawnerSettings")) Spawner = Settings;
    }
    if (!TestNotNull(TEXT("Узел деревьев в графе"), Trees) || !TestNotNull(TEXT("Спавнер в графе"), Spawner)) return false;
    TestEqual(TEXT("Узел знает плотность сэмплера"), Trees->SamplerTreesPer100SquareMeters, UPcgTreesSetupCommandlet::SamplerTreesPer100SquareMeters);

    const FObjectProperty* ParamsProperty = CastField<FObjectProperty>(Spawner->GetClass()->FindPropertyByName(TEXT("MeshSelectorParameters")));
    const UObject* Params = ParamsProperty ? ParamsProperty->GetObjectPropertyValue_InContainer(Spawner) : nullptr;
    const FNameProperty* Attribute = Params ? CastField<FNameProperty>(Params->GetClass()->FindPropertyByName(TEXT("AttributeName"))) : nullptr;
    TestTrue(TEXT("Спавнер берёт меш из атрибута узла"),
        Attribute && Attribute->GetPropertyValue_InContainer(Params) == Trees->MeshAttribute);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeTrees_TableFromJson,
    "Herbalist.BiomeTrees.TableFromJson",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeTrees_TableFromJson::RunTest(const FString& Parameters)
{
    UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
    Table->RowStruct = FHerbalistBiomeTreeRow::StaticStruct();
    const FString Good = TEXT("[{\"Name\":\"Taiga_Pine\",\"Biome\":\"Taiga\",\"Mesh\":\"/Engine/BasicShapes/Cone.Cone\",")
        TEXT("\"TreesPer100SquareMeters\":1.5,\"ScaleMin\":0.9,\"ScaleMax\":1.3,\"SpacingMeters\":3.5}]");
    TestEqual(TEXT("Одна строка"), UPcgTreesSetupCommandlet::FillTreeTable(Table, Good), 1);
    const FHerbalistBiomeTreeRow* Row = Table->FindRow<FHerbalistBiomeTreeRow>(TEXT("Taiga_Pine"), TEXT("Test"), false);
    if (!TestNotNull(TEXT("Строка на месте"), Row)) return false;
    TestTrue(TEXT("Биом разобран"), Row->Biome == EBiomeType::Taiga);
    TestEqual(TEXT("Плотность"), Row->TreesPer100SquareMeters, 1.5f, 1e-5f);
    TestEqual(TEXT("Шаг"), Row->SpacingMeters, 3.5f, 1e-5f);

    // Ошибка в строке -- таблица не тронута.
    const FString Bad = TEXT("[{\"Name\":\"X\",\"Biome\":\"Jungle\",\"Mesh\":\"/Engine/BasicShapes/Cone.Cone\",")
        TEXT("\"TreesPer100SquareMeters\":1,\"ScaleMin\":1,\"ScaleMax\":1,\"SpacingMeters\":1}]");
    AddExpectedError(TEXT("Строка деревьев"), EAutomationExpectedErrorFlags::Contains, 1);
    TestEqual(TEXT("Неизвестный биом -- отказ"), UPcgTreesSetupCommandlet::FillTreeTable(Table, Bad), -1);
    TestNotNull(TEXT("Прежняя строка осталась"), Table->FindRow<FHerbalistBiomeTreeRow>(TEXT("Taiga_Pine"), TEXT("Test"), false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeTrees_ProjectIsWired,
    "Herbalist.BiomeTrees.ProjectIsWired",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeTrees_ProjectIsWired::RunTest(const FString& Parameters)
{
    // После -run=PcgTreesSetup: у всех восьми биомов есть деревья, и сэмплер
    // не реже задуманного леса; граф в проекте; BP_BiomeVolume несёт
    // компонент с ним, генерация по запросу -- в игре граф не работает.
    const UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_BiomeTrees.DT_BiomeTrees"));
    if (!TestNotNull(TEXT("DT_BiomeTrees"), Table)) return false;
    TMap<EBiomeType, float> Density;
    Table->ForeachRow<FHerbalistBiomeTreeRow>(TEXT("Test"), [&Density](const FName&, const FHerbalistBiomeTreeRow& Row)
    {
        Density.FindOrAdd(Row.Biome) += Row.TreesPer100SquareMeters;
    });
    for (int32 Biome = 0; Biome <= static_cast<int32>(EBiomeType::Bog); ++Biome)
    {
        const float* Total = Density.Find(static_cast<EBiomeType>(Biome));
        const FString Name = UEnum::GetValueAsString(static_cast<EBiomeType>(Biome));
        TestTrue(FString::Printf(TEXT("У %s есть деревья"), *Name), Total && *Total > 0.0f);
        TestTrue(FString::Printf(TEXT("%s: сэмплер не реже леса"), *Name),
            !Total || *Total <= UPcgTreesSetupCommandlet::SamplerTreesPer100SquareMeters);
    }

    const UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, TEXT("/Game/PCG/PCG_Trees.PCG_Trees"));
    if (!TestNotNull(TEXT("PCG_Trees в проекте"), Graph)) return false;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/BP_BiomeVolume.BP_BiomeVolume"));
    if (!TestNotNull(TEXT("BP_BiomeVolume"), Blueprint) || !Blueprint->SimpleConstructionScript) return false;
    const UPCGComponent* Template = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        const UPCGComponent* Candidate = Node ? Cast<UPCGComponent>(Node->ComponentTemplate) : nullptr;
        if (Candidate && Candidate->GetGraph() == Graph) Template = Candidate;
    }
    if (!TestNotNull(TEXT("Компонент деревьев в BP_BiomeVolume"), Template)) return false;
    TestTrue(TEXT("Генерация по запросу"), Template->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnDemand);
    return true;
}

#endif
