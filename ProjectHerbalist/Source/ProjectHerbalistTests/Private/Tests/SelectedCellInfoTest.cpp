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

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
