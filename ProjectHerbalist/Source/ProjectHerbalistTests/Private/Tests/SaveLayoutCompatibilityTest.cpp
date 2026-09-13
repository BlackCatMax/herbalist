// Source/ProjectHerbalistTests/Private/Tests/SaveLayoutCompatibilityTest.cpp
//
// Разметка мира, этап 8 (2026-09-13) -- сейв и разметка. Координаты клеток в
// сейве имеют смысл только в той разметке, в которой записаны (решение
// пользователя 9): сейв другой разметки отклоняется с перечнем того, что
// поменялось. Границы ландшафта в разметку сейва не входят (решение 14):
// добавленные плитки сейв не ломают, клетки за убранными отбрасываются.
// Сама загрузка с диска (UHerbalistSaveSubsystem::LoadGame) требует
// UGameInstance, которого у мира редактора нет (довод в SaveSystemTest.cpp),
// поэтому проверяются функции, которые она зовёт, и сериализация сейва в
// память (UGameplayStatics::SaveGameToMemory, без GameInstance).

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Save/HerbalistSaveSubsystem.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Ландшафт с числами L_TestDev (квад 1 м, компонент 126 квадов, ячейка
    // стриминга 126 м) и полуразмером HalfExtentCm, кратным компоненту.
    // Клетка 9 м, страница 14 клеток, начало отсчёта (0, 0) -- при любом
    // полуразмере.
    FHerbalistWorldLayoutSource MakeSaveLayoutTestSource(double HalfExtentCm)
    {
        FHerbalistWorldLayoutSource Source;
        Source.bHasLandscape = true;
        Source.QuadSizeCm = 100.0;
        Source.ComponentSizeQuads = 126;
        Source.LandscapeOrigin = FVector2D(-HalfExtentCm, -HalfExtentCm);
        Source.LandscapeMin = FVector2D(-HalfExtentCm, -HalfExtentCm);
        Source.LandscapeMax = FVector2D(HalfExtentCm, HalfExtentCm);
        Source.bHasStreamingGrid = true;
        Source.StreamingGridName = FName(TEXT("MainPartition"));
        Source.StreamingCellSizeCm = 12600.0;
        Source.StreamingLoadingRangeCm = 25200.0;
        Source.StreamingGridOrigin = FVector2D::ZeroVector;
        return Source;
    }

    FHerbalistWorldLayout ResolveSaveLayoutTestSource(double HalfExtentCm)
    {
        TArray<FString> Warnings;
        return FWorldLayoutSolver::Resolve(MakeSaveLayoutTestSource(HalfExtentCm), FHerbalistWorldLayoutOverrides(), 100.0f, 30.0f, Warnings);
    }

    struct FScopedSaveLayoutRadius
    {
        UHerbalistSettings* Settings;
        float SavedRadiusMeters;

        FScopedSaveLayoutRadius()
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            Settings->ActiveSimulationRadiusMeters = 100.0f;
        }

        ~FScopedSaveLayoutRadius()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
        }
    };

    AGridWorldManager* SpawnSaveLayoutManager(UWorld* World, double HalfExtentCm)
    {
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            It->Destroy();
        }
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->BakedLayoutSource = MakeSaveLayoutTestSource(HalfExtentCm);
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }

    FSavedCellState MakeMarkedSavedCell(const FGridCell& Cell, float Purity)
    {
        FSavedCellState Saved;
        Saved.X = Cell.X;
        Saved.Y = Cell.Y;
        Saved.State = Cell.State;
        Saved.State.Meta.Purity = Purity;
        Saved.TargetState = Cell.TargetState;
        Saved.Memory = Cell.Memory;
        Saved.bResourcesSeeded = Cell.bResourcesSeeded;
        return Saved;
    }

    bool ExpectHash(FAutomationTestBase& Test, const TCHAR* What, uint32 Actual, uint32 Expected)
    {
        // TestTrue со значением в тексте: перегрузки TestEqual для uint32 у
        // FAutomationTestBase нет.
        return Test.TestTrue(FString::Printf(TEXT("%s: 0x%08X, ждали 0x%08X"), What, Actual, Expected), Actual == Expected);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_FingerprintIsStableAcrossBuilds,
    "Herbalist.WorldLayout.Save.FingerprintIsStableAcrossBuilds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_FingerprintIsStableAcrossBuilds::RunTest(const FString& Parameters)
{
    // Отпечаток пишется в сейв. Числа ниже посчитаны повтором HashCombine
    // (TypeHash.h) вне движка, дважды независимо. Сменились -- формула
    // отпечатка поменялась, и записанные числа в старых сейвах больше не
    // совпадают с новыми (совместимость от этого не страдает, она сверяется по
    // величинам, но лог и отладка врут).
    const uint32 TestDev = FWorldLayoutSolver::ComputeFingerprint(900.0, FVector2D::ZeroVector, 14);
    ExpectHash(*this, TEXT("Отпечаток L_TestDev (клетка 900 см, начало (0, 0), страница 14)"), TestDev, 0x3B59DADFu);
    ExpectHash(*this, TEXT("Начало (-450, 0) см -- отрицательная координата"),
        FWorldLayoutSolver::ComputeFingerprint(900.0, FVector2D(-450.0, 0.0), 14), 0xCE32881Eu);
    ExpectHash(*this, TEXT("Начало (450, 0) см"),
        FWorldLayoutSolver::ComputeFingerprint(900.0, FVector2D(450.0, 0.0), 14), 0x020F672Au);

    // Старшая половина int64: отрицательное число и число больше 2^32.
    ExpectHash(*this, TEXT("StableHashInt64(-45000)"), FWorldLayoutSolver::StableHashInt64(-45000), 0x85B2E525u);
    ExpectHash(*this, TEXT("StableHashInt64(45000)"), FWorldLayoutSolver::StableHashInt64(45000), 0x52E10BE5u);
    ExpectHash(*this, TEXT("StableHashInt64(2^33 + 7)"), FWorldLayoutSolver::StableHashInt64((int64(1) << 33) + 7), 0x1ABFF603u);

    const FHerbalistWorldLayout Layout = ResolveSaveLayoutTestSource(100800.0);
    if (TestTrue(TEXT("Разметка L_TestDev выведена"), Layout.bValid))
    {
        ExpectHash(*this, TEXT("...и её отпечаток тот же"), Layout.Fingerprint, 0x3B59DADFu);
    }

    ExpectHash(*this, TEXT("Шум double тоньше сотой сантиметра отпечаток не меняет"),
        FWorldLayoutSolver::ComputeFingerprint(900.0 + 1e-7, FVector2D(1e-7, -1e-7), 14), TestDev);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_SameLayoutIsCompatible,
    "Herbalist.WorldLayout.Save.SameLayoutIsCompatible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_SameLayoutIsCompatible::RunTest(const FString& Parameters)
{
    const FHerbalistWorldLayout Small = ResolveSaveLayoutTestSource(6300.0);
    const FHerbalistWorldLayout Large = ResolveSaveLayoutTestSource(25200.0);
    if (!TestTrue(TEXT("Обе разметки выведены"), Small.bValid && Large.bValid))
    {
        return false;
    }

    const FHerbalistSavedWorldLayout Saved = FWorldLayoutSolver::MakeSavedLayout(Small);
    TestTrue(TEXT("Разметка записана"), Saved.bValid);
    ExpectHash(*this, TEXT("...с отпечатком разметки"), Saved.Fingerprint, Small.Fingerprint);

    FString Reason;
    TestTrue(TEXT("Та же разметка совместима"), FWorldLayoutSolver::IsSaveCompatible(Saved, Small, Reason));
    TestTrue(TEXT("...без причины отказа"), Reason.IsEmpty());

    // Плитки добавили: сетка 56 x 56 от (-28, -28) вместо 28 x 28 от (-14, -14).
    TestTrue(TEXT("Ландшафт больше -- сетка другая"), Large.MinCell != Small.MinCell && Large.GridSize != Small.GridSize);
    TestTrue(TEXT("...а сейв совместим"), FWorldLayoutSolver::IsSaveCompatible(Saved, Large, Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_OtherLayoutIsRejectedWithReason,
    "Herbalist.WorldLayout.Save.OtherLayoutIsRejectedWithReason",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_OtherLayoutIsRejectedWithReason::RunTest(const FString& Parameters)
{
    const FHerbalistWorldLayout Current = ResolveSaveLayoutTestSource(6300.0);
    if (!TestTrue(TEXT("Разметка выведена"), Current.bValid))
    {
        return false;
    }
    const FHerbalistSavedWorldLayout Same = FWorldLayoutSolver::MakeSavedLayout(Current);

    FString Reason;

    FHerbalistSavedWorldLayout OtherCell = Same;
    OtherCell.CellSizeCm = 1000.0;
    TestFalse(TEXT("Другая клетка -- отказ"), FWorldLayoutSolver::IsSaveCompatible(OtherCell, Current, Reason));
    TestTrue(TEXT("...причина называет клетку и оба размера"), Reason.Contains(TEXT("клетка")) && Reason.Contains(TEXT("1000.00")) && Reason.Contains(TEXT("900.00")));

    FHerbalistSavedWorldLayout OtherAnchor = Same;
    OtherAnchor.Anchor = FVector2D(450.0, 0.0);
    TestFalse(TEXT("Другое начало отсчёта -- отказ"), FWorldLayoutSolver::IsSaveCompatible(OtherAnchor, Current, Reason));
    TestTrue(TEXT("...причина называет начало отсчёта"), Reason.Contains(TEXT("начало отсчёта")));

    FHerbalistSavedWorldLayout OtherPage = Same;
    OtherPage.PageSizeInCells = 28;
    TestFalse(TEXT("Другая страница -- отказ"), FWorldLayoutSolver::IsSaveCompatible(OtherPage, Current, Reason));
    TestTrue(TEXT("...причина называет страницу"), Reason.Contains(TEXT("страница 28")));

    FHerbalistSavedWorldLayout AllOther = OtherCell;
    AllOther.Anchor = OtherAnchor.Anchor;
    AllOther.PageSizeInCells = OtherPage.PageSizeInCells;
    TestFalse(TEXT("Всё другое -- отказ"), FWorldLayoutSolver::IsSaveCompatible(AllOther, Current, Reason));
    TestTrue(TEXT("...и все три отличия в причине"), Reason.Contains(TEXT("клетка")) && Reason.Contains(TEXT("начало отсчёта")) && Reason.Contains(TEXT("страница")));

    // Записанное число отпечатка не решает: сверяются величины.
    FHerbalistSavedWorldLayout StaleFingerprint = Same;
    StaleFingerprint.Fingerprint = Same.Fingerprint + 1;
    TestTrue(TEXT("Та же разметка с другим записанным отпечатком совместима"), FWorldLayoutSolver::IsSaveCompatible(StaleFingerprint, Current, Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_LayoutAndNoLayoutDoNotMix,
    "Herbalist.WorldLayout.Save.LayoutAndNoLayoutDoNotMix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_LayoutAndNoLayoutDoNotMix::RunTest(const FString& Parameters)
{
    const FHerbalistWorldLayout WithLayout = ResolveSaveLayoutTestSource(6300.0);
    const FHerbalistWorldLayout NoLayout;
    if (!TestTrue(TEXT("Разметка выведена"), WithLayout.bValid))
    {
        return false;
    }

    const FHerbalistSavedWorldLayout SavedWith = FWorldLayoutSolver::MakeSavedLayout(WithLayout);
    const FHerbalistSavedWorldLayout SavedWithout = FWorldLayoutSolver::MakeSavedLayout(NoLayout);
    TestFalse(TEXT("Карта без разметки пишет сейв без разметки"), SavedWithout.bValid);

    FString Reason;
    TestFalse(TEXT("Сейв с разметкой на карте без неё -- отказ"), FWorldLayoutSolver::IsSaveCompatible(SavedWith, NoLayout, Reason));
    TestTrue(TEXT("...причина: у карты разметки нет"), Reason.Contains(TEXT("у карты разметки нет")));

    TestFalse(TEXT("Сейв без разметки на карте с ней -- отказ"), FWorldLayoutSolver::IsSaveCompatible(FHerbalistSavedWorldLayout(), WithLayout, Reason));
    TestTrue(TEXT("...причина: сейв без разметки"), Reason.Contains(TEXT("без разметки")));

    TestTrue(TEXT("Оба без разметки -- совместимы, размер сетки сверяет загрузка"), FWorldLayoutSolver::IsSaveCompatible(SavedWithout, NoLayout, Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_LoadChecksRejectWithReason,
    "Herbalist.WorldLayout.Save.LoadChecksRejectWithReason",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_LoadChecksRejectWithReason::RunTest(const FString& Parameters)
{
    // Проверки, которые LoadGame делает до первой записи в мир: версия,
    // разметка, размер сетки.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedSaveLayoutRadius ScopedRadius;
    FString Reason;

    {
        AGridWorldManager* WithLayout = SpawnSaveLayoutManager(World, 6300.0);
        if (!TestNotNull(TEXT("Сетка с разметкой"), WithLayout))
        {
            return false;
        }
        UHerbalistSaveGame* Save = NewObject<UHerbalistSaveGame>();
        Save->SaveVersion = UHerbalistSaveSubsystem::CurrentSaveVersion;
        Save->WorldLayout = FWorldLayoutSolver::MakeSavedLayout(WithLayout->ResolvedLayout);
        Save->GridSizeX = 1;
        Save->GridSizeY = 1;
        TestTrue(TEXT("Та же разметка -- применим, размер сетки при разметке не сверяется"),
            UHerbalistSaveSubsystem::CheckSaveApplicable(*Save, *WithLayout, Reason));

        Save->SaveVersion = UHerbalistSaveSubsystem::CurrentSaveVersion + 1;
        TestFalse(TEXT("Версия новее сборки -- отказ"), UHerbalistSaveSubsystem::CheckSaveApplicable(*Save, *WithLayout, Reason));
        TestTrue(FString::Printf(TEXT("...причина называет версию (%s)"), *Reason), Reason.Contains(TEXT("newer")));

        Save->SaveVersion = 4;
        Save->WorldLayout = FHerbalistSavedWorldLayout();
        TestFalse(TEXT("Сейв v4 на карте с разметкой -- отказ"), UHerbalistSaveSubsystem::CheckSaveApplicable(*Save, *WithLayout, Reason));
        TestTrue(FString::Printf(TEXT("...причина -- версия до отпечатка (%s)"), *Reason), Reason.Contains(TEXT("v4")) && Reason.Contains(TEXT("до отпечатка разметки")));

        Save->SaveVersion = UHerbalistSaveSubsystem::CurrentSaveVersion;
        Save->WorldLayout = FWorldLayoutSolver::MakeSavedLayout(WithLayout->ResolvedLayout);
        Save->WorldLayout.PageSizeInCells = 28;
        TestFalse(TEXT("Другая разметка -- отказ"), UHerbalistSaveSubsystem::CheckSaveApplicable(*Save, *WithLayout, Reason));
        TestTrue(FString::Printf(TEXT("...причина называет страницу (%s)"), *Reason), Reason.Contains(TEXT("страница 28")));
        WithLayout->Destroy();
    }

    AGridWorldManager* Plain = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Сетка без разметки"), Plain))
    {
        return false;
    }
    UHerbalistSaveGame* PlainSave = NewObject<UHerbalistSaveGame>();
    PlainSave->SaveVersion = UHerbalistSaveSubsystem::CurrentSaveVersion;
    PlainSave->GridSizeX = Plain->GridSizeX;
    PlainSave->GridSizeY = Plain->GridSizeY;
    TestTrue(TEXT("Без разметки, тот же размер -- применим"), UHerbalistSaveSubsystem::CheckSaveApplicable(*PlainSave, *Plain, Reason));
    PlainSave->GridSizeX = Plain->GridSizeX + 1;
    TestFalse(TEXT("Без разметки, другой размер -- отказ"), UHerbalistSaveSubsystem::CheckSaveApplicable(*PlainSave, *Plain, Reason));
    TestTrue(FString::Printf(TEXT("...причина называет размер (%s)"), *Reason), Reason.Contains(TEXT("grid size mismatch")));
    Plain->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_SavedLayoutSurvivesSerialization,
    "Herbalist.WorldLayout.Save.SavedLayoutSurvivesSerialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_SavedLayoutSurvivesSerialization::RunTest(const FString& Parameters)
{
    // Та же сериализация, что у SaveGameToSlot, но в память -- без GameInstance.
    UHerbalistSaveGame* Save = NewObject<UHerbalistSaveGame>();
    TestFalse(TEXT("Новый сейв -- без разметки"), Save->WorldLayout.bValid);

    Save->SaveVersion = UHerbalistSaveSubsystem::CurrentSaveVersion;
    Save->WorldLayout.bValid = true;
    Save->WorldLayout.CellSizeCm = 900.25;
    Save->WorldLayout.Anchor = FVector2D(-450.0, 1234.56);
    Save->WorldLayout.PageSizeInCells = 14;
    Save->WorldLayout.Fingerprint = 0xCE32881Eu;

    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Сейв записан в память"), UGameplayStatics::SaveGameToMemory(Save, Bytes)))
    {
        return false;
    }
    const UHerbalistSaveGame* Loaded = Cast<UHerbalistSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Сейв прочитан из памяти"), Loaded))
    {
        return false;
    }
    TestEqual(TEXT("Версия"), Loaded->SaveVersion, UHerbalistSaveSubsystem::CurrentSaveVersion);
    TestTrue(TEXT("Разметка задана"), Loaded->WorldLayout.bValid);
    TestEqual(TEXT("Клетка (double)"), Loaded->WorldLayout.CellSizeCm, 900.25);
    TestTrue(TEXT("Начало (FVector2D)"), Loaded->WorldLayout.Anchor == FVector2D(-450.0, 1234.56));
    TestEqual(TEXT("Страница"), Loaded->WorldLayout.PageSizeInCells, 14);
    ExpectHash(*this, TEXT("Отпечаток (uint32 со старшим битом)"), Loaded->WorldLayout.Fingerprint, 0xCE32881Eu);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_CellsSurviveAddedAndRemovedTiles,
    "Herbalist.WorldLayout.Save.CellsSurviveAddedAndRemovedTiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_CellsSurviveAddedAndRemovedTiles::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedSaveLayoutRadius ScopedRadius;

    const float SmallPurity = 0.123456f;
    const float LargePurity = 0.654321f;

    // Сейв на малом ландшафте (±63 м): сетка 28 x 28 от (-14, -14).
    AGridWorldManager* Small = SpawnSaveLayoutManager(World, 6300.0);
    if (!TestNotNull(TEXT("Малая сетка"), Small))
    {
        return false;
    }
    const FGridCell* SmallCorner = Small->GetCellConst(-14, -14);
    if (!TestNotNull(TEXT("Клетка (-14, -14) малой сетки"), SmallCorner))
    {
        Small->Destroy();
        return false;
    }
    const FSavedCellState SavedOnSmall = MakeMarkedSavedCell(*SmallCorner, SmallPurity);
    const FHerbalistSavedWorldLayout SmallLayout = FWorldLayoutSolver::MakeSavedLayout(Small->ResolvedLayout);

    // Плитки добавили (±252 м): сетка 56 x 56 от (-28, -28), клетка (-14, -14)
    // -- та же земля.
    AGridWorldManager* Large = SpawnSaveLayoutManager(World, 25200.0);
    if (!TestNotNull(TEXT("Большая сетка"), Large))
    {
        return false;
    }
    TestTrue(TEXT("Большая сетка начинается с (-28, -28)"), Large->GetGridMinCell() == FIntPoint(-28, -28));
    FString Reason;
    TestTrue(TEXT("Сейв малой сетки совместим с большой"), FWorldLayoutSolver::IsSaveCompatible(SmallLayout, Large->ResolvedLayout, Reason));
    TestTrue(TEXT("Мировая точка клетки (-14, -14) не сдвинулась"),
        FVector2D(Large->GetCellWorldPositionFlat(-14, -14)).Equals(FVector2D(-12600.0, -12600.0), 0.01));
    TestEqual(TEXT("Ни одна клетка не отброшена"), Large->ApplySaveCells({ SavedOnSmall }), 0);
    const FGridCell* LargeLanded = Large->GetCellConst(-14, -14);
    if (TestNotNull(TEXT("Клетка (-14, -14) большой сетки"), LargeLanded))
    {
        TestEqual(TEXT("Клетка легла в (-14, -14)"), LargeLanded->State.Meta.Purity, SmallPurity, 1e-6f);
    }

    const FGridCell* LargeWest = Large->GetCellConst(-28, -28);
    const FGridCell* LargeEast = Large->GetCellConst(20, 20);
    const FGridCell* LargeInner = Large->GetCellConst(0, 0);
    if (!TestNotNull(TEXT("Клетка (-28, -28)"), LargeWest) || !TestNotNull(TEXT("Клетка (20, 20)"), LargeEast) || !TestNotNull(TEXT("Клетка (0, 0)"), LargeInner))
    {
        Large->Destroy();
        return false;
    }
    const TArray<FSavedCellState> SavedOnLarge = {
        MakeMarkedSavedCell(*LargeWest, LargePurity),
        MakeMarkedSavedCell(*LargeEast, LargePurity),
        MakeMarkedSavedCell(*LargeInner, LargePurity) };

    // Плитки убрали обратно. Перед загрузкой тронута клетка (5, 5), которой в
    // сейве нет, -- она откатывается к снимку после инициализации.
    Small = SpawnSaveLayoutManager(World, 6300.0);
    if (!TestNotNull(TEXT("Малая сетка снова"), Small))
    {
        return false;
    }
    const FGridCell* Touched = Small->GetCellConst(5, 5);
    if (!TestNotNull(TEXT("Клетка (5, 5)"), Touched))
    {
        Small->Destroy();
        return false;
    }
    const float TouchedDistortionBefore = Touched->State.Meta.Distortion;
    FStateDelta Delta;
    FGridCell TouchedChanged = *Touched;
    TouchedChanged.State.Meta.Distortion = FMath::Frac(TouchedDistortionBefore + 0.5f);
    Delta.WorldChanges.Add(FIntPoint(5, 5), TouchedChanged);
    Small->ApplyStateDelta(Delta);

    TestEqual(TEXT("Отброшены клетки с обеих сторон: (-28, -28) и (20, 20)"), Small->ApplySaveCells(SavedOnLarge), 2);
    const FGridCell* SmallInner = Small->GetCellConst(0, 0);
    if (TestNotNull(TEXT("Клетка (0, 0) малой сетки"), SmallInner))
    {
        TestEqual(TEXT("Клетка внутри легла в (0, 0)"), SmallInner->State.Meta.Purity, LargePurity, 1e-6f);
    }
    const FGridCell* TouchedAfter = Small->GetCellConst(5, 5);
    if (TestNotNull(TEXT("Клетка (5, 5) после загрузки"), TouchedAfter))
    {
        TestEqual(TEXT("Тронутая клетка без записи в сейве откатилась"), TouchedAfter->State.Meta.Distortion, TouchedDistortionBefore, 1e-6f);
    }
    TestEqual(TEXT("Сейв снимает только легшую клетку"), Small->CaptureSaveCells().Num(), 1);

    Small->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_SitesOutsideGridAreCounted,
    "Herbalist.WorldLayout.Save.SitesOutsideGridAreCounted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_SitesOutsideGridAreCounted::RunTest(const FString& Parameters)
{
    // Места за убранными плитками не роняют игру, но пропадают -- загрузка
    // считает их для лога.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedSaveLayoutRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnSaveLayoutManager(World, 6300.0);
    if (!TestNotNull(TEXT("Сетка 28 x 28 от (-14, -14)"), Manager))
    {
        return false;
    }

    FShrine Inside;
    Inside.Cell = FIntPoint(0, 0);
    FShrine Beyond;
    Beyond.Cell = FIntPoint(-28, 0);
    FShrine Unplaced;
    Manager->SetShrines({ Inside, Beyond, Unplaced });

    FEntityLandmark FarLandmark;
    FarLandmark.Cell = FIntPoint(20, 20);
    Manager->SetEntityLandmarks({ FarLandmark });

    Manager->SetTotemSite(FIntPoint(0, 30));
    Manager->SetSvetloyarSite(HerbalistCore::InvalidCell());
    Manager->SetGoryuchKamenSite(FIntPoint(1, 1));
    Manager->SetSoloveySite(HerbalistCore::InvalidCell());
    Manager->SetKalinovMostSite(HerbalistCore::InvalidCell());

    TestEqual(TEXT("За сеткой -- капище (-28, 0), ориентир (20, 20), Тотем (0, 30); незаданные не считаются"),
        UHerbalistSaveSubsystem::CountSitesOutsideGrid(*Manager), 3);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
