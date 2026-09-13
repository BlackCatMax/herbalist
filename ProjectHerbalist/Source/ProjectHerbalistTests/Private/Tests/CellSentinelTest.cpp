// Source/ProjectHerbalistTests/Private/Tests/CellSentinelTest.cpp
//
// Разметка мира, этап 6 (2026-09-13) -- координата «клетка не задана». С
// глобальными координатами клеток от начала сетки World Partition (решение
// пользователя 13) (-1, -1) -- настоящая клетка, поэтому «не задано» теперь
// HerbalistCore::InvalidCell().

#include "Core/Types/HerbalistCellCoord.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Shrine/ShrineTypes.h"
#include "Core/Journal/JournalTypes.h"
#include "Core/Zaryana/MemoryFragmentTypes.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/World/GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSentinel_InvalidCellIsNotARealCoordinate,
    "Herbalist.WorldLayout.GlobalCoord.InvalidCellIsNotARealCoordinate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSentinel_InvalidCellIsNotARealCoordinate::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("(-1,-1) -- настоящая координата клетки"), HerbalistCore::IsValidCell(FIntPoint(-1, -1)));
    TestFalse(TEXT("InvalidCell -- не координата"), HerbalistCore::IsValidCell(HerbalistCore::InvalidCell()));

    // Настоящая функция расстояния, а не арифметика в тесте (ревью): разность
    // координат с самой далёкой клеткой не переполняется, и незаданная клетка
    // не оказывается «рядом» с настоящей.
    const FIntPoint FarthestCell(1 << 28, 1 << 28);
    FShrine FarShrine;
    FarShrine.Cell = FarthestCell;
    FarShrine.Restoration = 1.0f;
    TestNull(TEXT("Капище на краю мира не влияет на незаданную клетку"),
        HerbalistCore::Shrine::FindDominantShrine(HerbalistCore::InvalidCell(), { FarShrine }, 3));

    FShrine UnsetShrine;
    UnsetShrine.Restoration = 1.0f;
    TestNull(TEXT("Незаданное капище не влияет на клетку на краю мира"),
        HerbalistCore::Shrine::FindDominantShrine(FarthestCell, { UnsetShrine }, 3));

    FShrine MinusOneShrine;
    MinusOneShrine.Cell = FIntPoint(-1, -1);
    MinusOneShrine.Restoration = 1.0f;
    TestNotNull(TEXT("Капище в (-1,-1) -- настоящее: влияет на соседнюю (0,0)"),
        HerbalistCore::Shrine::FindDominantShrine(FIntPoint(0, 0), { MinusOneShrine }, 1));

    TestEqual(TEXT("Журнал: незаданная клетка -- прочерк"), HerbalistCore::CellToDisplayString(HerbalistCore::InvalidCell()), FString(TEXT("—")));
    TestEqual(TEXT("Журнал: (-1,-1) -- как есть"), HerbalistCore::CellToDisplayString(FIntPoint(-1, -1)), FString(TEXT("(-1,-1)")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSentinel_DefaultsAreInvalidCell,
    "Herbalist.WorldLayout.GlobalCoord.DefaultsAreInvalidCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSentinel_DefaultsAreInvalidCell::RunTest(const FString& Parameters)
{
    // Всё, что раньше по умолчанию было (-1,-1) или не задавалось вовсе,
    // теперь InvalidCell. Страховка от регрессий: новый код, взявший -1 или
    // (0,0) как «не задано», упрётся в эти проверки.
    TestTrue(TEXT("FShrine::Cell"), FShrine().Cell == HerbalistCore::InvalidCell());
    TestTrue(TEXT("FEntityLandmark::Cell"), FEntityLandmark().Cell == HerbalistCore::InvalidCell());
    TestTrue(TEXT("FHerbalistBase::Cell"), FHerbalistBase().Cell == HerbalistCore::InvalidCell());
    TestTrue(TEXT("FJournalEntry::Cell"), FJournalEntry().Cell == HerbalistCore::InvalidCell());
    TestTrue(TEXT("FActiveMemoryFragment::Cell"), FActiveMemoryFragment().Cell == HerbalistCore::InvalidCell());

    // Сейв пишется с дельтой против умолчаний класса: незаданное место в сейве
    // не записано и при загрузке получит это значение.
    const UHerbalistSaveGame* SaveDefaults = GetDefault<UHerbalistSaveGame>();
    TestTrue(TEXT("Сейв: Тотем"), SaveDefaults->TotemSite == HerbalistCore::InvalidCell());
    TestTrue(TEXT("Сейв: Светлояр"), SaveDefaults->SvetloyarSite == HerbalistCore::InvalidCell());
    TestTrue(TEXT("Сейв: Горюч-камень"), SaveDefaults->GoryuchKamenSite == HerbalistCore::InvalidCell());
    TestTrue(TEXT("Сейв: Соловей"), SaveDefaults->SoloveySite == HerbalistCore::InvalidCell());
    TestTrue(TEXT("Сейв: Калинов мост"), SaveDefaults->KalinovMostSite == HerbalistCore::InvalidCell());

    const AGridWorldManager* ManagerDefaults = GetDefault<AGridWorldManager>();
    TestTrue(TEXT("Менеджер до посева: Соловей не размещён"), ManagerDefaults->GetSoloveySite() == HerbalistCore::InvalidCell());
    TestTrue(TEXT("Менеджер до посева: Горюч-камень не размещён"), ManagerDefaults->GetGoryuchKamenSite() == HerbalistCore::InvalidCell());

    // Найдено ревью: у ресурса «не назначено» было GridX == -1, у стола клетка
    // не инициализировалась вовсе.
    TestEqual(TEXT("Ресурс: клетка по X не назначена"), GetDefault<AHerbalistResourceActor>()->GetGridX(), HerbalistCore::InvalidCellCoord);
    TestEqual(TEXT("Ресурс: клетка по Y не назначена"), GetDefault<AHerbalistResourceActor>()->GetGridY(), HerbalistCore::InvalidCellCoord);
    TestTrue(TEXT("Стол алхимии вне сетки -- InvalidCell"), GetDefault<AAlchemyTableActor>()->GetGridCoords() == HerbalistCore::InvalidCell());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSentinel_MissAndEmptySelectionGiveInvalidCell,
    "Herbalist.WorldLayout.GlobalCoord.MissAndEmptySelectionGiveInvalidCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSentinel_MissAndEmptySelectionGiveInvalidCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    int32 X = 0;
    int32 Y = 0;
    const bool bInside = Manager->WorldPositionToCell(FVector(-1.0e7, -1.0e7, 0.0), X, Y);
    TestFalse(TEXT("Точка далеко за сеткой -- не клетка"), bInside);
    TestTrue(TEXT("Промах отдаёт InvalidCell, а не (-1,-1)"), FIntPoint(X, Y) == HerbalistCore::InvalidCell());

    FString ResourceName;
    float RegrowthTimer = 0.0f;
    float Distortion = 0.0f;
    float HarvestStress = 0.0f;
    Manager->GetSelectedCellInfoBP(X, Y, ResourceName, RegrowthTimer, Distortion, HarvestStress);
    TestTrue(TEXT("Без выбора клетки -- InvalidCell"), FIntPoint(X, Y) == HerbalistCore::InvalidCell());
    TestEqual(TEXT("Без выбора клетки -- ресурса нет"), ResourceName, FString(TEXT("None")));

    // GetCellFromHit говорит о промахе через bool (ревью): вызывающие больше
    // не проверяют X < 0.
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (TestNotNull(TEXT("Контроллер заспавнен"), PC))
    {
        FHitResult Miss;
        Miss.Location = FVector(-1.0e7, -1.0e7, 0.0);
        TestFalse(TEXT("Попадание за сеткой -- false"), PC->GetCellFromHit(Miss, X, Y));
        TestTrue(TEXT("...и InvalidCell"), FIntPoint(X, Y) == HerbalistCore::InvalidCell());

        FHitResult Hit;
        Hit.Location = Manager->GetCellWorldPositionFlat(3, 4) + FVector(Manager->CellSize * 0.5f, Manager->CellSize * 0.5f, 0.0f);
        TestTrue(TEXT("Попадание в клетку (3,4) -- true"), PC->GetCellFromHit(Hit, X, Y) && X == 3 && Y == 4);
        PC->Destroy();
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSentinel_BrewFragmentNeedsARealCell,
    "Herbalist.WorldLayout.GlobalCoord.BrewFragmentNeedsARealCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSentinel_BrewFragmentNeedsARealCell::RunTest(const FString& Parameters)
{
    // Найдено ревью: варка без стола передаёт клетку InvalidCell, и фрагмент
    // «Первая варка» вставал в (-5e11, -5e11) см -- не подобрать, а слот
    // активного фрагмента занят на всё время жизни и откат.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->TryTriggerCoherentBrewFragment(HerbalistCore::InvalidCell(), /*Coherence=*/1.0f, /*Distortion=*/0.0f, /*Purity=*/1.0f);
    TestEqual(TEXT("Варка без клетки -- фрагмент не появился"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));

    // Контроль: та же варка в настоящей клетке фрагмент даёт -- значит, отказ
    // выше дала клетка, а не пороги.
    Manager->TryTriggerCoherentBrewFragment(FIntPoint(5, 5), 1.0f, 0.0f, 1.0f);
    TestEqual(TEXT("Та же варка в клетке (5,5) -- фрагмент появился"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("PERVAYA_VARKA")));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
