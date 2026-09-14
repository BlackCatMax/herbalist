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

    // Точки интереса, засеянные при старте, закрепляют свои страницы (ревью
    // 2026-09-13) -- тесты выгрузки их убирают, как ориентиры и капища.
    void ClearPointsOfInterestForSaveLayoutTest(AGridWorldManager* Manager)
    {
        Manager->SetTotemSite(HerbalistCore::InvalidCell());
        Manager->SetSvetloyarSite(HerbalistCore::InvalidCell());
        Manager->SetGoryuchKamenSite(HerbalistCore::InvalidCell());
        Manager->SetSoloveySite(HerbalistCore::InvalidCell());
        Manager->SetKalinovMostSite(HerbalistCore::InvalidCell());
    }

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

// Места за убранными плитками ландшафта живут (решение пользователя
// 2026-09-13, вариант «Страницы мест остаются»): загрузка расширяет сетку до
// целых страниц с местами, страницы мест собираются из основы и закрепляются.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_SitesBeyondRemovedTilesKeepTheirPages,
    "Herbalist.WorldLayout.Save.SitesBeyondRemovedTilesKeepTheirPages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_SitesBeyondRemovedTilesKeepTheirPages::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    // Все клетки -- блочный фолбэк: проверяется, что его основа не зависит от
    // размера сетки после расширения.
    for (TActorIterator<ABiomeRegionVolume> It(World); It; ++It)
    {
        It->Destroy();
    }
    FScopedSaveLayoutRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnSaveLayoutManager(World, 6300.0);
    if (!TestNotNull(TEXT("Сетка 28 x 28 от (-14, -14)"), Manager))
    {
        return false;
    }
    // Места, засеянные при старте, закрепляют свои страницы -- их заменяют места сейва.
    Manager->SetEntityLandmarks({});
    Manager->SetShrines({});
    Manager->SetLegendaryAnchorsForTests({});
    ClearPointsOfInterestForSaveLayoutTest(Manager);

    // Клетка, тронутая до расширения: её линейный индекс пересчитывается.
    const FGridCell* ToTouch = Manager->GetCellConst(5, 5);
    if (!TestNotNull(TEXT("Клетка (5, 5)"), ToTouch))
    {
        Manager->Destroy();
        return false;
    }
    const float UntouchedDistortion = ToTouch->State.Meta.Distortion;
    FStateDelta Delta;
    FGridCell Touched = *ToTouch;
    Touched.State.Meta.Distortion = 0.61f;
    Delta.WorldChanges.Add(FIntPoint(5, 5), Touched);
    Manager->ApplyStateDelta(Delta);

    int32 SummaryChunksBefore = 0;
    Manager->ForEachChunkSummary([&SummaryChunksBefore](const FHerbalistChunkSummary&) { ++SummaryChunksBefore; });
    TestEqual(TEXT("Сводок у сетки ландшафта -- 4 x 4 чанка"), SummaryChunksBefore, 16);

    // Сейв, записанный до уборки плиток: капище (-22, 3) с соседями на той же
    // странице, капище (0, 5) у края страницы внутри сетки, Тотем (0, 30),
    // Светлояр (-20, 25); клетка капища и клетка страницы-заполнителя (-10, 30).
    UHerbalistSaveGame* Save = NewObject<UHerbalistSaveGame>();
    FShrine BeyondShrine;
    BeyondShrine.Cell = FIntPoint(-22, 3);
    BeyondShrine.Restoration = 0.4f;
    Save->Shrines.Add(BeyondShrine);
    FShrine EdgeShrine;
    EdgeShrine.Cell = FIntPoint(0, 5);
    Save->Shrines.Add(EdgeShrine);
    Save->TotemSite = FIntPoint(0, 30);
    Save->SvetloyarSite = FIntPoint(-20, 25);
    FSavedCellState ShrineCellSaved;
    ShrineCellSaved.X = -22;
    ShrineCellSaved.Y = 3;
    ShrineCellSaved.State.Meta.Distortion = 0.66f;
    Save->Cells.Add(ShrineCellSaved);
    FSavedCellState FillerCellSaved;
    FillerCellSaved.X = -10;
    FillerCellSaved.Y = 30;
    Save->Cells.Add(FillerCellSaved);

    // Страницы мест: (-2, 0), (0, 2), (-2, 1) -- таблица X -2..0, Y -1..2.
    // Сосед (-1, 5) капища (0, 5) лежит в сетке, но и сосед у края сетки
    // ландшафта расширять её не должен -- капище внутри сетки соседей не даёт.
    FShrine WorldEdgeShrine;
    WorldEdgeShrine.Cell = FIntPoint(13, 0);
    const TArray<FIntPoint> EdgeOnly = UHerbalistSaveSubsystem::CollectSaveSiteCells(*NewObject<UHerbalistSaveGame>(), *Manager);
    UHerbalistSaveGame* EdgeSave = NewObject<UHerbalistSaveGame>();
    EdgeSave->Shrines.Add(WorldEdgeShrine);
    TestFalse(TEXT("Сосед (14, 0) капища на краю ландшафта не входит в места"),
        UHerbalistSaveSubsystem::CollectSaveSiteCells(*EdgeSave, *Manager).Contains(FIntPoint(14, 0)));
    TestEqual(TEXT("Пустой сейв -- только незаданные точки интереса"), EdgeOnly.Num(), 5);

    TestEqual(TEXT("Три страницы мест"), Manager->EnsureGridCoversSites(UHerbalistSaveSubsystem::CollectSaveSiteCells(*Save, *Manager)), 3);
    TestEqual(TEXT("Сетка от (-28, -14)"), Manager->GetGridMinCell(), FIntPoint(-28, -14));
    TestEqual(TEXT("42 клетки по X"), Manager->GridSizeX, 42);
    TestEqual(TEXT("56 клеток по Y"), Manager->GridSizeY, 56);
    TestTrue(TEXT("Страница (-1, 2) -- заполнитель"), Manager->IsCellInExtensionFiller(-10, 30));
    TestFalse(TEXT("Страница места -- не заполнитель"), Manager->IsCellInExtensionFiller(-20, 25));
    TestFalse(TEXT("Сетка ландшафта -- не заполнитель"), Manager->IsCellInExtensionFiller(5, 5));

    // Заполнитель не размывает биомный граф: сводок -- ландшафт и три страницы
    // мест по 2 x 2 чанка.
    int32 SummaryChunksAfter = 0;
    Manager->ForEachChunkSummary([&SummaryChunksAfter](const FHerbalistChunkSummary&) { ++SummaryChunksAfter; });
    TestEqual(TEXT("Сводок -- 16 чанков ландшафта и 12 чанков страниц мест"), SummaryChunksAfter, 28);

    const TArray<FSavedCellState> CapturedAfterExtension = Manager->CaptureSaveCells();
    const FSavedCellState* TouchedSaved = CapturedAfterExtension.FindByPredicate([](const FSavedCellState& Saved) { return Saved.X == 5 && Saved.Y == 5; });
    if (TestNotNull(TEXT("Тронутая до расширения клетка по-прежнему в сейве"), TouchedSaved))
    {
        TestEqual(TEXT("...со своим состоянием"), TouchedSaved->State.Meta.Distortion, 0.61f, 1e-6f);
    }

    Manager->SetShrines(Save->Shrines);
    Manager->SetTotemSite(Save->TotemSite);
    Manager->SetSvetloyarSite(Save->SvetloyarSite);
    TestEqual(TEXT("Отброшена только клетка страницы-заполнителя"), Manager->ApplySaveCells(Save->Cells), 1);
    TestEqual(TEXT("Мест за сеткой нет"), UHerbalistSaveSubsystem::CountSitesOutsideGrid(*Manager), 0);
    const FGridCell* TouchedAfterLoad = Manager->GetCellConst(5, 5);
    if (TestNotNull(TEXT("Клетка (5, 5) после загрузки"), TouchedAfterLoad))
    {
        TestEqual(TEXT("Тронутая до расширения клетка без записи в сейве откатилась по пересчитанному индексу"),
            TouchedAfterLoad->State.Meta.Distortion, UntouchedDistortion, 1e-6f);
    }

    const FGridCell* ShrineCell = Manager->GetCellConst(-22, 3);
    if (TestNotNull(TEXT("Клетка капища загружена"), ShrineCell))
    {
        TestEqual(TEXT("...с сохранённым состоянием"), ShrineCell->State.Meta.Distortion, 0.66f, 1e-6f);
    }
    TestNotNull(TEXT("Клетка Тотема загружена"), Manager->GetCellConst(0, 30));
    const FGridCell* SvetloyarCell = Manager->GetCellConst(-20, 25);
    const TArray<EBiomeType> AllBiomes = FBiomeDefaults::GetAllBiomeTypes();
    if (TestNotNull(TEXT("Клетка Светлояра загружена"), SvetloyarCell) && AllBiomes.Num() > 0)
    {
        // Блок (-4, 5): ширина строки блоков до расширения 28 / 5 = 5 даёт блок
        // 21, после расширения было бы 42 / 5 = 8 -- блок 36.
        const int32 BiomeCount = AllBiomes.Num();
        const int32 Block = HerbalistCore::FloorDivCoord(25, 5) * (28 / 5) + HerbalistCore::FloorDivCoord(-20, 5);
        TestEqual(TEXT("Фолбэк биома новой страницы -- по ширине сетки до расширения"),
            SvetloyarCell->Biome, AllBiomes[(Block % BiomeCount + BiomeCount) % BiomeCount]);
    }

    // Тотем снят из текущего состояния: его страницу за краем держит только
    // закрепление страниц мест расширения.
    Manager->SetTotemSite(HerbalistCore::InvalidCell());

    // Земля только под страницей (-1, -1), зритель там же. Страница (0, -1) без
    // земли и без мест выгружается; страницы мест и страница (-1, 0) с соседом
    // (-1, 5) капища (0, 5) -- закреплены. Активный чанк (-3, -2) лежит на
    // заполнителе (-2, -1) -- стриминг его не грузит.
    Manager->SetGroundCoverageForTests({ FBox2D(FVector2D(-12600.0, -12600.0), FVector2D(0.0, 0.0)) });
    Manager->SetActiveChunkCentersForTests({ FIntPoint(-2, -2) });
    Manager->CatchUpActivatedChunks();
    TestNull(TEXT("Заполнитель (-2, -1) у активного зрителя не загружен"), Manager->GetCellConst(-20, -10));
    TestNull(TEXT("Страница (0, -1) без земли и мест выгружена"), Manager->GetCellConst(5, -5));
    TestNotNull(TEXT("Страница соседа капища закреплена"), Manager->GetCellConst(-5, 5));
    TestNotNull(TEXT("Страница капища закреплена"), Manager->GetCellConst(-22, 3));
    TestNotNull(TEXT("Страница Тотема закреплена"), Manager->GetCellConst(0, 30));
    TestNotNull(TEXT("Страница Светлояра закреплена"), Manager->GetCellConst(-20, 25));
    Manager->SetActiveChunkCentersForTests({});
    Manager->ClearGroundCoverageForTests();

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_GridExtensionKeepsUnloadedDeltas,
    "Herbalist.WorldLayout.Save.GridExtensionKeepsUnloadedDeltas",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_GridExtensionKeepsUnloadedDeltas::RunTest(const FString& Parameters)
{
    // Дельта клетки выгруженной страницы лежит по линейному индексу --
    // расширение сетки обязано её пересчитать.
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
    Manager->SetEntityLandmarks({});
    Manager->SetShrines({});
    Manager->SetLegendaryAnchorsForTests({});
    ClearPointsOfInterestForSaveLayoutTest(Manager);

    const FGridCell* ToTouch = Manager->GetCellConst(13, 13);
    if (!TestNotNull(TEXT("Клетка (13, 13)"), ToTouch))
    {
        Manager->Destroy();
        return false;
    }
    FStateDelta Delta;
    FGridCell Touched = *ToTouch;
    Touched.State.Meta.Distortion = 0.77f;
    Delta.WorldChanges.Add(FIntPoint(13, 13), Touched);
    Manager->ApplyStateDelta(Delta);

    Manager->SetGroundCoverageForTests({ FBox2D(FVector2D(-12600.0, -12600.0), FVector2D(0.0, 0.0)) });
    Manager->SetActiveChunkCentersForTests({ FIntPoint(-2, -2) });
    Manager->CatchUpActivatedChunks();
    TestNull(TEXT("Страница (0, 0) выгружена"), Manager->GetCellConst(13, 13));
    TestEqual(TEXT("Дельта тронутой клетки"), Manager->GetUnloadedCellDeltaCountForTests(), 1);

    TestEqual(TEXT("Страница места (-28, -28)"), Manager->EnsureGridCoversSites({ FIntPoint(-28, -28) }), 1);
    TestEqual(TEXT("Сетка 42 x 42 от (-28, -28)"), FIntPoint(Manager->GridSizeX, Manager->GridSizeY), FIntPoint(42, 42));

    Manager->SetGroundCoverageForTests({ FBox2D(FVector2D(-12600.0, -12600.0), FVector2D(12600.0, 12600.0)) });
    Manager->SetActiveChunkCentersForTests({ FIntPoint(1, 1) });
    Manager->CatchUpActivatedChunks();
    const FGridCell* TouchedAfter = Manager->GetCellConst(13, 13);
    if (TestNotNull(TEXT("Клетка (13, 13) снова загружена"), TouchedAfter))
    {
        TestEqual(TEXT("Дельта встала на свою клетку"), TouchedAfter->State.Meta.Distortion, 0.77f, 1e-6f);
    }
    const TArray<FSavedCellState> Captured = Manager->CaptureSaveCells();
    TestTrue(TEXT("Клетка по-прежнему тронута"),
        Captured.ContainsByPredicate([](const FSavedCellState& Saved) { return Saved.X == 13 && Saved.Y == 13; }));

    Manager->SetActiveChunkCentersForTests({});
    Manager->ClearGroundCoverageForTests();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveLayout_SitesInsideGridDoNotExtendIt,
    "Herbalist.WorldLayout.Save.SitesInsideGridDoNotExtendIt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveLayout_SitesInsideGridDoNotExtendIt::RunTest(const FString& Parameters)
{
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
    TestEqual(TEXT("Место в сетке и незаданное -- страниц не добавлено"),
        Manager->EnsureGridCoversSites({ FIntPoint(0, 0), HerbalistCore::InvalidCell() }), 0);
    TestEqual(TEXT("Сетка прежняя"), FIntPoint(Manager->GridSizeX, Manager->GridSizeY), FIntPoint(28, 28));
    TestEqual(TEXT("Начало прежнее"), Manager->GetGridMinCell(), FIntPoint(-14, -14));

    // Место за 1,8 км: прямоугольник 28 x ~200 тыс. клеток больше предела сетки
    // (4 млн) -- расширения нет, сетка не тронута.
    TestEqual(TEXT("Место за пределом размера сетки не восстановлено"),
        Manager->EnsureGridCoversSites({ FIntPoint(0, 200000) }), 0);
    TestEqual(TEXT("...и сетка прежняя"), FIntPoint(Manager->GridSizeX, Manager->GridSizeY), FIntPoint(28, 28));
    Manager->Destroy();

    // Без разметки сетка ручная и не расширяется.
    AGridWorldManager* Plain = SpawnAndBeginPlay(World);
    if (TestNotNull(TEXT("Менеджер без разметки"), Plain))
    {
        TestEqual(TEXT("Без разметки -- не расширяется"), Plain->EnsureGridCoversSites({ FIntPoint(-100, -100) }), 0);
        TestFalse(TEXT("...и место остаётся за сеткой"), Plain->IsCellInGrid(-100, -100));
        Plain->Destroy();
    }
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
