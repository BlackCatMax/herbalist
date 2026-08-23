// Source/ProjectHerbalistTests/Private/Tests/BistabilityTest.cpp
//
// Бистабильная релаксация клетки (обсуждение в сессии 2026-08-24) — общий
// случай того, что раньше делали только Гнильники для Болота: выше порога
// входа Corruption цель релаксации сама сдвигается к испорченному полюсу
// (естественное восстановление невозможно, только усугубляет), ниже порога
// выхода — возвращается к здоровому умолчанию биома. DispatchBeginPlay-
// паттерн — тот же, что уже обкатан в SaveSystemTest.cpp/ShrineTest.cpp.

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBistability_CrossingEnterThresholdFlipsTargetToCorruptPole,
    "Herbalist.Bistability.CrossingEnterThresholdFlipsTargetToCorruptPole",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBistability_CrossingEnterThresholdFlipsTargetToCorruptPole::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }
    Cell->Biome = EBiomeType::Taiga;
    Cell->bIsWater = false;
    Cell->State.Meta.Corruption = 0.9f;   // выше порога входа (0.85 по умолчанию)
    Cell->TargetState.Meta.Corruption = 0.18f;   // ещё не пересчитана — здоровое умолчание Тайги
    Cell->Memory.bDegrading = false;

    Manager->RegenerateCellParameters(0.016f);

    TestTrue(TEXT("Cell flips into degrading regime"), Cell->Memory.bDegrading);
    TestEqual(TEXT("TargetState.Corruption snaps to the corrupt pole"), Cell->TargetState.Meta.Corruption, 1.0f);
    TestEqual(TEXT("TargetState.Purity snaps to zero"), Cell->TargetState.Meta.Purity, 0.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBistability_PlayerInterventionBelowExitThresholdRestoresHealthyTarget,
    "Herbalist.Bistability.PlayerInterventionBelowExitThresholdRestoresHealthyTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBistability_PlayerInterventionBelowExitThresholdRestoresHealthyTarget::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }
    Cell->Biome = EBiomeType::Taiga;
    Cell->bIsWater = false;

    // Уже деградирует (как если бы клетка давно перевалила порог входа).
    Cell->State.Meta.Corruption = 0.9f;
    Cell->TargetState.Meta.Corruption = 1.0f;
    Cell->TargetState.Meta.Purity = 0.0f;
    Cell->Memory.bDegrading = true;

    // Игрок активно продавил порчу ниже порога выхода (0.65) — например,
    // применил очищающее зелье прямо на клетку.
    Cell->State.Meta.Corruption = 0.5f;

    Manager->RegenerateCellParameters(0.016f);

    // Не сверяем с конкретными числами Тайги (0.18/0.80) — FBiomeDefaults::
    // BiomeDataTable заполняет только ProjectHerbalistGameModeBase::BeginPlay,
    // которого в этом тестовом окружении (голый editor-мир, без реальной игровой
    // сессии) не происходит; GetDefaultState() честно вернёт FRealState() по
    // умолчанию — то есть Purity/Stability "здорового" отката совпадут с 0.0
    // из sick pole чисто случайно, для них тест был бы ложноположительным.
    // Corruption различим железно (0.0 отката vs 1.0 полюса) — этого достаточно,
    // чтобы проверить сам факт переключения регистра; точные цифры уже
    // покрыты компендиум-экстракцией отдельно.
    TestFalse(TEXT("Cell exits degrading regime"), Cell->Memory.bDegrading);
    TestNotEqual(TEXT("TargetState.Corruption no longer pinned to the corrupt pole (1.0)"), Cell->TargetState.Meta.Corruption, 1.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBistability_HysteresisPreventsFlickerInTheMiddleBand,
    "Herbalist.Bistability.HysteresisPreventsFlickerInTheMiddleBand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBistability_HysteresisPreventsFlickerInTheMiddleBand::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }
    Cell->Biome = EBiomeType::Taiga;
    Cell->bIsWater = false;

    // Внутри полосы гистерезиса (0.65..0.85) — тот самый диапазон, где по
    // умолчанию сидит здоровое Болото (Corruption 0.70, DT_BiomeDefaults.json):
    // намеренно неоднозначная зона, не должна дёргаться сама по себе.
    Cell->State.Meta.Corruption = 0.70f;
    Cell->TargetState.Meta.Corruption = 0.18f;   // как будто клетка была здорова
    Cell->Memory.bDegrading = false;

    Manager->RegenerateCellParameters(0.016f);

    TestFalse(TEXT("Mid-band value doesn't trip a healthy cell into degrading"), Cell->Memory.bDegrading);
    TestEqual(TEXT("TargetState untouched while regime doesn't flip"), Cell->TargetState.Meta.Corruption, 0.18f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
