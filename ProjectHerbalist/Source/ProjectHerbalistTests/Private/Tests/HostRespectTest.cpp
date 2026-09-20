// Source/ProjectHerbalistTests/Private/Tests/HostRespectTest.cpp
//
// «Хозяин» травы (решение пользователя 2026-09-20): Respect Основного, чья
// трава, множит её шанс вырасти (1 + HostRespectSuitabilityWeight × Respect).
// Хозяин -- ближайший экземпляр этого Основного в биоме клетки; чужой биом
// траву не множит.

#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "TestWorldHelpers.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    // Доля выпадений HostedHerb из 2000 бросков против травы без хозяина с тем
    // же весом и той же пригодностью.
    float HostedShare(float Respect, bool bHostKnown)
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();
        FIngredientTableRow Hosted;
        Hosted.AllowedBiomes = { EBiomeType::Bog };
        Hosted.HostEntityID = FName(TEXT("Кикимора болотная"));
        Table->AddRow(FName(TEXT("HostedHerb")), Hosted);
        FIngredientTableRow Free;
        Free.AllowedBiomes = { EBiomeType::Bog };
        Table->AddRow(FName(TEXT("FreeHerb")), Free);

        UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(OwnerGameInstance);
        Registry->LoadFromDataTable(Table);

        FGridCell Cell;
        Cell.Biome = EBiomeType::Bog;
        FHarvestContext Context;
        if (bHostKnown)
        {
            Context.HostRespect.Add(FName(TEXT("Кикимора болотная")), Respect);
        }

        FRandomStream Rng(911);
        int32 HostedCount = 0;
        const int32 Trials = 2000;
        for (int32 i = 0; i < Trials; ++i)
        {
            HostedCount += Registry->GetRandomResourceForBiome(Cell, Context, Rng) == FName(TEXT("HostedHerb")) ? 1 : 0;
        }
        Registry->Reset();
        return static_cast<float>(HostedCount) / Trials;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHostRespect_RespectShiftsHostedHerbChance,
    "Herbalist.HostRespect.RespectShiftsHostedHerbChance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHostRespect_RespectShiftsHostedHerbChance::RunTest(const FString& Parameters)
{
    // Вес 0.3: благосклонный хозяин -- 1.3 : 1 (доля ~0.565), разгневанный --
    // 0.7 : 1 (~0.41), нейтральный и неизвестный -- поровну.
    const float Blessed = HostedShare(1.0f, true);
    const float Angry = HostedShare(-1.0f, true);
    const float Neutral = HostedShare(0.0f, true);
    const float Unknown = HostedShare(0.0f, false);

    TestTrue(FString::Printf(TEXT("Благосклонный хозяин -- его трава чаще (%.3f)"), Blessed), Blessed > 0.53f && Blessed < 0.60f);
    TestTrue(FString::Printf(TEXT("Разгневанный -- реже (%.3f)"), Angry), Angry > 0.37f && Angry < 0.45f);
    TestTrue(FString::Printf(TEXT("Нейтральный -- поровну (%.3f)"), Neutral), FMath::Abs(Neutral - 0.5f) < 0.04f);
    TestTrue(FString::Printf(TEXT("Хозяин не найден -- без множителя (%.3f)"), Unknown), FMath::Abs(Unknown - 0.5f) < 0.04f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHostRespect_ContextTakesNearestHostOfCellBiome,
    "Herbalist.HostRespect.ContextTakesNearestHostOfCellBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHostRespect_ContextTakesNearestHostOfCellBiome::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(5, 5);
    if (!TestNotNull(TEXT("Cell (5,5) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::ForestSteppe;
    Cell->BiomeWeights.Reset();

    auto MakeLandmark = [](const TCHAR* ID, int32 X, int32 Y, float Respect)
    {
        FEntityLandmark Landmark;
        Landmark.EntityID = FName(ID);
        Landmark.Cell = FIntPoint(X, Y);
        Landmark.Respect = Respect;
        return Landmark;
    };
    // Два Полевика (Лесостепь): ближний благосклонен, дальний разгневан.
    // Кикимора -- хозяин Болота, в лесостепной клетке её быть не должно.
    // Домовой -- ручная регистрация, хозяином трав не считается.
    const TArray<FEntityLandmark> Saved = Manager->GetEntityLandmarks();
    Manager->SetEntityLandmarks({
        MakeLandmark(TEXT("Полевик"), 15, 15, -0.8f),
        MakeLandmark(TEXT("Полевик"), 6, 5, 0.6f),
        MakeLandmark(TEXT("Кикимора болотная"), 5, 6, 1.0f),
        MakeLandmark(TEXT("Домовой"), 5, 5, 1.0f) });

    const FHarvestContext Context = Manager->BuildHarvestContextForCell(*Cell);
    const float* Polevik = Context.HostRespect.Find(FName(TEXT("Полевик")));
    if (TestNotNull(TEXT("Полевик -- хозяин лесостепной клетки"), Polevik))
    {
        TestEqual(TEXT("Взят ближний Полевик"), *Polevik, 0.6f);
    }
    TestFalse(TEXT("Кикимора -- хозяин другого биома"), Context.HostRespect.Contains(FName(TEXT("Кикимора болотная"))));
    TestFalse(TEXT("Домовой -- не хозяин трав"), Context.HostRespect.Contains(FName(TEXT("Домовой"))));

    Manager->SetEntityLandmarks(Saved);
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
