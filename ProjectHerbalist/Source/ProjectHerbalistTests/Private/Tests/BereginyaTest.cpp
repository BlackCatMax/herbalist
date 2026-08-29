// Source/ProjectHerbalistTests/Private/Tests/BereginyaTest.cpp
//
// Берегиня (Легендарный, Речная пойма) не имела ни одного автотеста до этой
// сессии. Реконсиляция с 16_Entity_Manifestation.md §16.4 (2026-08-24):
// раньше единственным триггером был Memory.HistoryPurity — "упрощённая
// версия без капищ", написанная до того, как капища появились в проекте.
// Теперь капища есть, и §16.4 прямо описывает второй путь ("высокая
// Restoration капища поблизости"), который был добавлен как OR к первому,
// не замена. Эти тесты проверяют оба пути независимо: если бы второй путь
// был реализован неверно (например, "и" вместо "или", или не читал бы
// реальный Shrines массив), один из двух тестов ниже упал бы.
// DispatchBeginPlay-паттерн — тот же, что BistabilityTest.cpp/ShrineTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FGridCell* SetupFloodplainWaterCell(AGridWorldManager* Manager)
    {
        FGridCell* Cell = Manager->GetCell(0, 0);
        if (!Cell) return nullptr;
        Cell->Biome = EBiomeType::Floodplain;
        Cell->bIsWater = true;
        Cell->Memory.HistoryPurity = 0.0f;   // ниже порога 0.75 -- этот путь молчит
        return Cell;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBereginya_HistoryPurityAloneStillTriggers,
    "Herbalist.Bereginya.HistoryPurityAloneStillTriggers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBereginya_HistoryPurityAloneStillTriggers::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = SetupFloodplainWaterCell(Manager);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }

    Cell->Memory.HistoryPurity = 0.9f;   // выше дефолтного порога 0.75
    Manager->SetShrines({});             // явно нет ни одного капища рядом

    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Берегиня manifests from sustained HistoryPurity alone, no shrine needed"),
        Cell->ManifestedEntityID, FName(TEXT("Берегиня")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBereginya_NearbyShrineTriggersEvenWithLowHistoryPurity,
    "Herbalist.Bereginya.NearbyShrineTriggersEvenWithLowHistoryPurity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBereginya_NearbyShrineTriggersEvenWithLowHistoryPurity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = SetupFloodplainWaterCell(Manager);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }

    // HistoryPurity остаётся на 0.0 (см. SetupFloodplainWaterCell) -- если бы
    // §16.4 второй путь не работал, этот тест бы не прошёл.
    FShrine NearbyShrine;
    NearbyShrine.Cell = FIntPoint(1, 1);   // Чебышёвское расстояние 1 -- внутри дефолтного радиуса 3
    NearbyShrine.Type = EShrineType::Water;
    NearbyShrine.Restoration = 0.9f;       // выше дефолтного порога 0.7
    Manager->SetShrines({ NearbyShrine });

    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Берегиня manifests from a well-restored nearby shrine even with zero HistoryPurity"),
        Cell->ManifestedEntityID, FName(TEXT("Берегиня")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBereginya_NeitherPathEligibleMeansNoManifestation,
    "Herbalist.Bereginya.NeitherPathEligibleMeansNoManifestation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBereginya_NeitherPathEligibleMeansNoManifestation::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = SetupFloodplainWaterCell(Manager);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }

    Manager->SetShrines({});   // низкий HistoryPurity (из SetupFloodplainWaterCell) и нет капищ

    Manager->UpdateEntityManifestations(1.0f);

    TestNotEqual(TEXT("Берегиня does not manifest when neither trigger path is eligible"),
        Cell->ManifestedEntityID, FName(TEXT("Берегиня")));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
