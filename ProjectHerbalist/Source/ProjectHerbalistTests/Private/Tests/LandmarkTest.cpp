// Source/ProjectHerbalistTests/Private/Tests/LandmarkTest.cpp
//
// "Хозяева" места (16_Entity_Manifestation.md §16.3), 2026-08-29: Respect
// мигрирован с пассивной проверки HarvestStress/Purity клетки на подношение
// (Apply-на-клетку-обиталище), тем же каналом и знаковым принципом, что уже
// есть у капищ (GridWorldManagerTick.cpp, RunSimulationStep), но без спада
// при небрежении — по прямому решению пользователя.
//
// Тестируется отдельно от самого триггера подношения (Apply -> Delta.
// WorldChanges -> Landmark.Respect): построить полный Command/Pipeline/Delta
// цикл в голом тестовом мире означало бы либо мокать Pipeline, либо гонять
// PIE-сессию — тот же компромисс, на который уже пошёл ShrineTest.cpp для
// идентичного по структуре капищного подношения ("Интеграция с реальным
// пайплайном... проверена вручную построчно, отдельных автотестов нет",
// 15_Cycles_And_Shrines.md §15.6). Здесь проверяется то, что реально ново
// и дёшево тестируется напрямую: (а) FindLandmarkAt, (б) что проявление
// (благословение/порча) корректно читает Landmark.Respect, откуда бы он ни
// взялся — та же дисциплина, что у остальных Ambient-тестов в этой сессии
// (précondition через прямую установку состояния, не через полный пайплайн).
// DispatchBeginPlay-паттерн — тот же, что AmbientEntityTest.cpp/ShrineTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    AGridWorldManager* SpawnAndBeginPlay(UWorld* World)
    {
        if (!World) return nullptr;
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistLandmark_FindLandmarkAtLocatesByExactCell,
    "Herbalist.Landmark.FindLandmarkAtLocatesByExactCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistLandmark_FindLandmarkAtLocatesByExactCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FEntityLandmark Landmark;
    Landmark.EntityID = FName(TEXT("Полевик"));
    Landmark.Cell = FIntPoint(4, 7);
    Landmark.Respect = 0.0f;
    Manager->SetEntityLandmarks({ Landmark });

    TestNull(TEXT("No landmark at an unrelated cell"), Manager->FindLandmarkAt(FIntPoint(0, 0)));
    FEntityLandmark* Found = Manager->FindLandmarkAt(FIntPoint(4, 7));
    if (TestNotNull(TEXT("Landmark found at its exact cell"), Found))
    {
        TestEqual(TEXT("Found landmark has the right EntityID"), Found->EntityID, Landmark.EntityID);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistLandmark_HighRespectBlessesTheFieldsPotencyAndPurity,
    "Herbalist.Landmark.HighRespectBlessesTheFieldsPotencyAndPurity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistLandmark_HighRespectBlessesTheFieldsPotencyAndPurity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(2, 2);
    if (!TestNotNull(TEXT("Cell (2,2) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::ForestSteppe;
    Cell->bIsWater = false;
    Cell->TargetState.Meta.Potency = 0.3f;
    Cell->TargetState.Meta.Purity  = 0.3f;

    // Respect уже высок -- как если бы за него уже поднесли не раз. Не через
    // Apply/пайплайн (см. комментарий у файла), напрямую -- проверяем только
    // потребление значения, не его накопление.
    FEntityLandmark Landmark;
    Landmark.EntityID = FName(TEXT("Полевик"));
    Landmark.Cell = FIntPoint(2, 2);
    Landmark.Respect = 0.8f;   // выше порога благословения (0.5)
    Manager->SetEntityLandmarks({ Landmark });

    const float PotencyBefore = Cell->TargetState.Meta.Potency;
    const float PurityBefore  = Cell->TargetState.Meta.Purity;
    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Полевик manifests as blessed"), Cell->ManifestedEntityID, FName(TEXT("Полевик")));
    TestTrue(TEXT("TargetState.Potency nudged up"), Cell->TargetState.Meta.Potency > PotencyBefore);
    TestTrue(TEXT("TargetState.Purity nudged up"), Cell->TargetState.Meta.Purity > PurityBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistLandmark_LowRespectCursesTheFieldsStability,
    "Herbalist.Landmark.LowRespectCursesTheFieldsStability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistLandmark_LowRespectCursesTheFieldsStability::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(3, 3);
    if (!TestNotNull(TEXT("Cell (3,3) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::ForestSteppe;
    Cell->bIsWater = false;
    Cell->TargetState.Meta.Stability = 0.5f;

    FEntityLandmark Landmark;
    Landmark.EntityID = FName(TEXT("Полевик"));
    Landmark.Cell = FIntPoint(3, 3);
    Landmark.Respect = -0.6f;   // ниже порога порчи (-0.3)
    Manager->SetEntityLandmarks({ Landmark });

    const float StabilityBefore = Cell->TargetState.Meta.Stability;
    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Полевик manifests as cursed"), Cell->ManifestedEntityID, FName(TEXT("Полевик")));
    TestTrue(TEXT("TargetState.Stability nudged down"), Cell->TargetState.Meta.Stability < StabilityBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistLandmark_RespectDoesNotDecayPassively,
    "Herbalist.Landmark.RespectDoesNotDecayPassively",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistLandmark_RespectDoesNotDecayPassively::RunTest(const FString& Parameters)
{
    // Регрессия на миграцию 2026-08-29: раньше высокий HarvestStress клетки
    // сам по себе опускал Respect каждый тик. Теперь Respect не двигается
    // вообще без подношения -- даже если клетка сильно истощена.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(6, 6);
    if (!TestNotNull(TEXT("Cell (6,6) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::ForestSteppe;
    Cell->bIsWater = false;
    Cell->HarvestStress = 1.0f;   // максимально истощена -- раньше это опускало Respect

    FEntityLandmark Landmark;
    Landmark.EntityID = FName(TEXT("Полевик"));
    Landmark.Cell = FIntPoint(6, 6);
    Landmark.Respect = 0.2f;
    Manager->SetEntityLandmarks({ Landmark });

    Manager->UpdateEntityManifestations(10.0f);   // крупный DeltaTime -- если бы спад остался, был бы заметен

    const TArray<FEntityLandmark>& Landmarks = Manager->GetEntityLandmarks();
    if (TestEqual(TEXT("Exactly one landmark"), Landmarks.Num(), 1))
    {
        TestEqual(TEXT("Respect untouched by HarvestStress alone"), Landmarks[0].Respect, 0.2f);
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
