// Source/ProjectHerbalistTests/Private/Tests/ResourceCellRegistrationTest.cpp
//
// Растение числится ровно в одной клетке (2026-09-27, по PIE-логу:
// «Resource=None x10», «ресурсов 47» на клетке 9 м). SpawnActor проигрывает
// BeginPlay раньше Init(): BeginPlay ставил актор на клетку по позиции
// (ближайший угол), Init() -- на клетку сетки (растение со слота стоит
// внутри клетки, WorldPositionToCell), и растение числилось и у соседа. Сбор снимал его с одной клетки, у соседа
// оставалась мёртвая запись, сейв соседа выращивал копию. В мире редактора
// SpawnActor BeginPlay не проигрывает -- порядок воспроизводится руками.

#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceCell_InitMovesOffTheCellFoundByPosition,
    "Herbalist.Resources.CellRegistration.InitMovesOffTheCellFoundByPosition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceCell_InitMovesOffTheCellFoundByPosition::RunTest(const FString& Parameters)
{
    // Порядок игры: SpawnActor -> BeginPlay (клетка по позиции) -> Init
    // (клетка сетки). Растение стоит там, куда его поставил BeginPlay, а
    // сетка заводит его на другую клетку -- после Init оно только в ней, и
    // сбор не оставляет мёртвой записи.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* ByPosition = Manager->GetCell(4, 4);
    FGridCell* ByGrid = Manager->GetCell(2, 2);
    if (!TestTrue(TEXT("Клетки есть"), ByPosition && ByGrid)) { Manager->Destroy(); return false; }

    const FVector Pos = Manager->GetCellWorldPositionFlat(4, 4);
    AHerbalistResourceActor* Plant = World->SpawnActor<AHerbalistResourceActor>(
        AHerbalistResourceActor::StaticClass(), Pos, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Растение"), Plant)) { Manager->Destroy(); return false; }
    Plant->SetWorldManager(Manager);
    Plant->DispatchBeginPlay();
    if (!TestTrue(TEXT("Sanity: BeginPlay поставил на клетку по позиции"), ByPosition->ResourceActors.Contains(Plant)))
    {
        Plant->Destroy();
        Manager->Destroy();
        return false;
    }

    Plant->Init(FName(TEXT("bol_01")), FText::GetEmpty(), nullptr, FRealState(), Pos, Manager, 2, 2, 1.0f, false, false);
    TestTrue(TEXT("Числится в клетке сетки"), ByGrid->ResourceActors.Contains(Plant));
    TestFalse(TEXT("Снят с клетки по позиции"), ByPosition->ResourceActors.Contains(Plant));

    Manager->OnResourceCollected(Plant);
    TestFalse(TEXT("Собрано -- в клетке сетки нет"), ByGrid->ResourceActors.Contains(Plant));
    const bool bAnyEntryLeft = ByPosition->ResourceActors.Contains(Plant) || ByGrid->ResourceActors.Contains(Plant);
    TestFalse(TEXT("Мёртвой записи не осталось нигде"), bAnyEntryLeft);

    Plant->Destroy();
    Manager->Destroy();
    return true;
}

#endif
