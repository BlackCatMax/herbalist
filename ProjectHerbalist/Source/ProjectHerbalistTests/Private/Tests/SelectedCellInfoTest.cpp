// Source/ProjectHerbalistTests/Private/Tests/SelectedCellInfoTest.cpp
//
// GetSelectedCellInfo() -- отладочная строка правого клика/Info(),
// единственный консольный способ проверить состояние конкретной клетки
// без остановки игры. Найдено 2026-09-03 при проверке плана (пункт 0.5,
// вода как отдельный регион): вода не даёт ResourceActor, поэтому клетка
// с bIsWater=true печаталась как "Resource=None" -- неотличимо от обычной
// пустой суши. Пользователь кликал по клеткам вокруг капища, ища воду
// именно этим инструментом, и инструмент был к ней слеп.

#include "Core/World/GridWorldManager.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSelectedCellInfo_WaterCellIsDistinguishableFromEmptyLand,
    "Herbalist.SelectedCellInfo.WaterCellIsDistinguishableFromEmptyLand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSelectedCellInfo_WaterCellIsDistinguishableFromEmptyLand::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* DryCell = Manager->GetCell(0, 0);
    FGridCell* WaterCell = Manager->GetCell(1, 0);
    if (!TestNotNull(TEXT("Dry cell exists"), DryCell) || !TestNotNull(TEXT("Water cell exists"), WaterCell))
    {
        Manager->Destroy();
        return false;
    }

    WaterCell->bIsWater = true;
    WaterCell->WaterTypeID = FName(TEXT("TestBogWater"));

    Manager->SelectCell(0, 0);
    const FString DryInfo = Manager->GetSelectedCellInfo();

    Manager->SelectCell(1, 0);
    const FString WaterInfo = Manager->GetSelectedCellInfo();

    TestTrue(TEXT("Dry cell reports no resource"), DryInfo.Contains(TEXT("Resource=None")));
    TestFalse(TEXT("Water cell does NOT read as an ordinary empty cell"), WaterInfo.Contains(TEXT("Resource=None")));
    TestTrue(TEXT("Water cell names its water type"), WaterInfo.Contains(TEXT("TestBogWater")));

    Manager->Destroy();
    return true;
}

// Найдено 2026-09-04 при разборе жалобы "почему некоторые квадраты
// полностью пустые внутри биома?" -- в приложенном логе та же клетка
// печаталась как один ресурс ("Resource=bol_12"), хотя реально в ней стояло
// 123 актора: строка показывала только Cell->ResourceActors[0], а не длину
// массива. Считать по клику приходилось открытием аутлайнера -- этот тест
// проверяет, что счётчик виден прямо в строке.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSelectedCellInfo_ShowsResourceCount,
    "Herbalist.SelectedCellInfo.ShowsResourceCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSelectedCellInfo_ShowsResourceCount::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Три актора на одну клетку -- Init() сам регистрируется в
    // Cell.ResourceActors (RegisterOnCell), тот же путь, что и настоящий
    // спавн, без нужды поднимать IngredientRegistrySubsystem (недоступен в
    // редакторском тестовом мире, см. TestWorldHelpers.h).
    for (int32 i = 0; i < 3; ++i)
    {
        AHerbalistResourceActor* Resource = World->SpawnActor<AHerbalistResourceActor>();
        if (!TestNotNull(TEXT("Resource actor spawned"), Resource)) { Manager->Destroy(); return false; }
        Resource->Init(FName(TEXT("bol_12")), FText::FromString(TEXT("Зверобой")), nullptr,
            FRealState(), FVector::ZeroVector, Manager, 3, 4, 0.0f, false, false);
    }

    Manager->SelectCell(3, 4);
    const FString Info = Manager->GetSelectedCellInfo();

    TestTrue(FString::Printf(TEXT("Info names the resource (got '%s')"), *Info), Info.Contains(TEXT("bol_12")));
    TestTrue(FString::Printf(TEXT("Info shows the real count, not just the first actor (got '%s')"), *Info),
        Info.Contains(TEXT("x3")));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
