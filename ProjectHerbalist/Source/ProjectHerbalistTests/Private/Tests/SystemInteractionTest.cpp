// Source/ProjectHerbalistTests/Private/Tests/SystemInteractionTest.cpp
//
// "Смотрим, насколько реально мешают столкновения систем на практике"
// (обсуждение 2026-08-30, вопрос про глобального директора). Не юнит-тест
// одной системы -- намеренно СТАЛКИВАЕТ несколько независимых писателей
// TargetState на одной и соседних клетках одновременно (бистабильность +
// Моховые духи + заражение соседей + влияние биом-графа) и гоняет много
// реальных тиков (RegenerateCellParameters + UpdateEntityManifestations
// вместе, как в Tick()), чтобы увидеть эмпирически, а не по чтению кода:
// сходится ли итог к чему-то осмысленному, не улетает ли в NaN/за пределы
// [0,1], не мигает ли ManifestedEntityID каждый тик.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSystemInteraction_StackedWritersConvergeWithoutChaos,
    "Herbalist.SystemInteraction.StackedWritersConvergeWithoutChaos",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSystemInteraction_StackedWritersConvergeWithoutChaos::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День -- не хотим ночного/рассветного нуджа поверх

    // Клетка-мишень: Тайга, ОДНОВРЕМЕННО за порогом входа бистабильности
    // (Corruption > 0.85) и за порогом Моховых духов (Purity > 0.80) --
    // Purity/Corruption независимые оси по дизайну (см. HerbalistCoreMath.h),
    // обе высокие сразу -- не ошибка ввода, а намеренно редкий, но легальный
    // случай, который стоит проверить.
    FGridCell* Target = Manager->GetCell(10, 10);
    // Соседняя заражающая клетка -- уже на полюсе, будет весь прогон толкать Target.
    FGridCell* Contagious = Manager->GetCell(10, 9);
    if (!TestNotNull(TEXT("Target cell exists"), Target) || !TestNotNull(TEXT("Contagious neighbor exists"), Contagious))
    {
        Manager->Destroy();
        return false;
    }

    Target->Biome = EBiomeType::Taiga;
    Target->bIsWater = false;
    Target->State.Meta.Corruption = 0.95f;
    Target->State.Meta.Purity = 0.85f;
    Target->State.Meta.Stability = 0.5f;
    Target->State.Meta.Distortion = 0.2f;
    Target->TargetState = Target->State;
    Target->Memory.bDegrading = false;   // ещё не зафиксирована -- зафиксируется первым же тиком

    Contagious->Biome = EBiomeType::Bog;
    Contagious->bIsWater = false;
    Contagious->Memory.bDegrading = true;
    Contagious->State.Meta.Corruption = 1.0f;
    Contagious->TargetState.Meta.Corruption = 1.0f;
    Contagious->TargetState.Meta.Purity = 0.0f;
    Contagious->TargetState.Meta.Distortion = 1.0f;
    Contagious->TargetState.Meta.Stability = 0.0f;

    // 200 тиков по 1 секунде -- достаточно, чтобы релаксация (StressRecoveryGameDays-
    // масштаба медленная) и гистерезис успели сказать своё слово, не только
    // мгновенный первый кадр.
    bool bSawNaNOrOutOfRange = false;
    int32 ManifestFlips = 0;
    FName PrevManifest = Target->ManifestedEntityID;

    for (int32 i = 0; i < 200; ++i)
    {
        Manager->RegenerateCellParameters(1.0f);
        Manager->UpdateEntityManifestations(1.0f);

        const FRealState& S = Target->State;
        const FRealState& T = Target->TargetState;
        auto InRange = [](float v) { return FMath::IsFinite(v) && v >= -0.0001f && v <= 1.0001f; };
        if (!InRange(S.Meta.Corruption) || !InRange(S.Meta.Purity) || !InRange(S.Meta.Distortion) || !InRange(S.Meta.Stability)
            || !InRange(T.Meta.Corruption) || !InRange(T.Meta.Purity) || !InRange(T.Meta.Distortion) || !InRange(T.Meta.Stability))
        {
            bSawNaNOrOutOfRange = true;
        }

        if (Target->ManifestedEntityID != PrevManifest)
        {
            ++ManifestFlips;
            PrevManifest = Target->ManifestedEntityID;
        }
    }

    TestFalse(TEXT("State/TargetState never leaves [0,1] or produces NaN under stacked writers"), bSawNaNOrOutOfRange);

    // Не "никогда не переключается" (гистерезис допускает единичный переход,
    // когда бистабильность один раз побеждает), а "не дребезжит каждый тик" --
    // если что-то мигает по нескольку раз за 200 тиков, это и есть тот самый
    // практический вред от столкновения систем, который стоило бы разобрать
    // отдельно перед тем, как решать про директора.
    TestTrue(FString::Printf(TEXT("ManifestedEntityID does not flicker uncontrollably (%d flips over 200 ticks)"), ManifestFlips),
        ManifestFlips <= 2);

    // Смысловая проверка исхода: заражение (Corruption+) толкает клетку в ту
    // же сторону, что и её собственная бистабильность -- при таком заведомо
    // перекошенном старте (Corruption 0.95 против Purity 0.85, плюс
    // непрерывный внешний толчок) естественно ожидать, что клетка в итоге
    // зафиксируется на испорченном полюсе, а не останется хозяйством Моховых
    // духов -- сама бистабильность работает по Corruption, заражение усиливает
    // именно эту сторону, ничто не толкает Corruption обратно вниз.
    TestTrue(TEXT("Cell ends up locked onto the corrupt pole (contagion + self bistability both push that way)"),
        Target->Memory.bDegrading);

    AddInfo(FString::Printf(TEXT("Final state: ManifestedEntityID=%s bDegrading=%d State.Purity=%.3f State.Corruption=%.3f TargetState.Purity=%.3f TargetState.Corruption=%.3f"),
        *Target->ManifestedEntityID.ToString(), Target->Memory.bDegrading,
        Target->State.Meta.Purity, Target->State.Meta.Corruption,
        Target->TargetState.Meta.Purity, Target->TargetState.Meta.Corruption));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
