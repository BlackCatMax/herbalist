// Source/ProjectHerbalistTests/Private/Tests/ShrineTest.cpp
//
// Капища v1 (02_GDD/15_Cycles_And_Shrines.md §15.5). GetInfluenceAt — чистая
// функция, тестируется без мира/актора. Регистрация и спад Restoration нужен
// живой AGridWorldManager — тот же DispatchBeginPlay-паттерн, что уже
// обкатан в SaveSystemTest.cpp.

#include "Core/Shrine/ShrineTypes.h"
#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrine_InfluenceRespectsRadius,
    "Herbalist.Shrine.InfluenceRespectsRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrine_InfluenceRespectsRadius::RunTest(const FString& Parameters)
{
    TArray<FShrine> Shrines;
    FShrine S;
    S.Cell = FIntPoint(10, 10);
    S.Restoration = 0.8f;
    Shrines.Add(S);

    TestEqual(TEXT("On the shrine's own cell"), HerbalistCore::Shrine::GetInfluenceAt(FIntPoint(10, 10), Shrines, 3), 0.8f);
    TestEqual(TEXT("At the edge of the radius (Chebyshev 3)"), HerbalistCore::Shrine::GetInfluenceAt(FIntPoint(13, 10), Shrines, 3), 0.8f);
    TestEqual(TEXT("Just outside the radius"), HerbalistCore::Shrine::GetInfluenceAt(FIntPoint(14, 10), Shrines, 3), 0.0f);
    TestEqual(TEXT("Diagonally outside the radius (Chebyshev, not Manhattan)"), HerbalistCore::Shrine::GetInfluenceAt(FIntPoint(13, 13), Shrines, 3), 0.8f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrine_OverlapTakesStrongestNotSum,
    "Herbalist.Shrine.OverlapTakesStrongestNotSum",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrine_OverlapTakesStrongestNotSum::RunTest(const FString& Parameters)
{
    // Два капища рядом не должны давать сумму влияния — §15.5 говорит про
    // одно капище, не про сложение (см. комментарий у GetInfluenceAt).
    TArray<FShrine> Shrines;
    FShrine A; A.Cell = FIntPoint(0, 0); A.Restoration = 0.3f;
    FShrine B; B.Cell = FIntPoint(1, 0); B.Restoration = 0.6f;
    Shrines.Add(A);
    Shrines.Add(B);

    TestEqual(TEXT("Strongest overlapping shrine wins, not the sum"), HerbalistCore::Shrine::GetInfluenceAt(FIntPoint(0, 0), Shrines, 3), 0.6f);
    return true;
}

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrine_RegisterIsIdempotent,
    "Herbalist.Shrine.RegisterIsIdempotent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrine_RegisterIsIdempotent::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->RegisterShrine(FIntPoint(5, 5), EShrineType::Ancestral);
    Manager->RegisterShrine(FIntPoint(5, 5), EShrineType::Water);   // повторная регистрация — не дубликат, просто меняет тип

    TestEqual(TEXT("Registering twice at the same cell doesn't duplicate"), Manager->GetShrines().Num(), 1);
    FShrine* Found = Manager->FindShrineAt(FIntPoint(5, 5));
    if (TestNotNull(TEXT("Shrine found at its cell"), Found))
    {
        TestEqual(TEXT("Second registration updates the type"), Found->Type, EShrineType::Water);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrine_NeglectDecaysTowardZeroFromBothSides,
    "Herbalist.Shrine.NeglectDecaysTowardZeroFromBothSides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrine_NeglectDecaysTowardZeroFromBothSides::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->RegisterShrine(FIntPoint(1, 1), EShrineType::Ancestral);
    Manager->RegisterShrine(FIntPoint(2, 2), EShrineType::Ancestral);
    FShrine* Blessed = Manager->FindShrineAt(FIntPoint(1, 1));
    FShrine* Desecrated = Manager->FindShrineAt(FIntPoint(2, 2));
    if (!TestNotNull(TEXT("Blessed shrine found"), Blessed) || !TestNotNull(TEXT("Desecrated shrine found"), Desecrated))
    {
        Manager->Destroy();
        return false;
    }
    Blessed->Restoration = 0.1f;
    Desecrated->Restoration = -0.1f;

    // 28 суток * 32 мин/сутки * 60 сек — полный горизонт спада (§15.5). Один
    // большой шаг вместо реального времени: UpdateShrines детерминирован по
    // DeltaTime, не по количеству вызовов.
    const float FullDecayHorizonSeconds = 28.0f * 32.0f * 60.0f;
    Manager->UpdateShrines(FullDecayHorizonSeconds);

    TestEqual(TEXT("Fully neglected blessing decays exactly to zero, not below"), Blessed->Restoration, 0.0f);
    TestEqual(TEXT("Fully neglected desecration decays exactly to zero, not below"), Desecrated->Restoration, 0.0f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
