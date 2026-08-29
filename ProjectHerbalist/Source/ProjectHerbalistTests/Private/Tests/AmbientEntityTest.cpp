// Source/ProjectHerbalistTests/Private/Tests/AmbientEntityTest.cpp
//
// Регрессия для AmbientEntityTypes.h + AGridWorldManager::UpdateEntityManifestations
// после рефакторинга 2026-08-24: Гнильники (единственный Низший до этой
// сессии) вынесены из захардкоженного if-блока в таблицу определений,
// добавлены Моховые духи (Тайга) и Степные огни (Степь) — этот файл
// проверяет, что (а) старое поведение Гнильников не изменилось и (б) новые
// определения действительно применяются через тот же обобщённый цикл.
// 2026-08-29: добавлены Кувшинкины духи/Ледяные духи/Суховейки (сезонный
// гейт, Potency/Resonance/Magnitude-нудж) — и тест на смежную регрессию:
// с появлением второго определения на одном биоме (Степные огни + Суховейки,
// оба Степь) цикл больше не может прерываться на первом совпадении Biome.
// DispatchBeginPlay-паттерн — тот же, что BistabilityTest.cpp/ShrineTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_GnilnikiStillManifestsAfterRefactor,
    "Herbalist.AmbientEntity.GnilnikiStillManifestsAfterRefactor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_GnilnikiStillManifestsAfterRefactor::RunTest(const FString& Parameters)
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
    Cell->Biome = EBiomeType::Bog;
    Cell->bIsWater = false;
    Cell->State.Meta.Corruption = 0.8f;   // выше дефолтного порога 0.6
    Cell->State.Meta.Purity = 0.5f;
    // TargetState по умолчанию FRealState() = всё в нуле (BiomeDataTable не
    // заполнен в голом тестовом мире, см. комментарий в BistabilityTest.cpp) —
    // нудж вниз от 0.0 не отличим от "не сработало" (кламп к 0.0), поэтому
    // задаём правдоподобное ненулевое начальное значение явно.
    Cell->TargetState.Meta.Purity = 0.5f;
    // GameClockSeconds по умолчанию 0.0 -- с 2026-08-29 это ещё и Рассвет
    // (§15.2, DayCycleTest.cpp), который сам поднимает Purity и почти
    // отменял бы нудж Гнильников вниз чистым совпадением чисел. Явно ставим
    // середину Дня, чтобы тест проверял ровно Гнильников, не их гонку с Рассветом.
    Manager->SetGameClockSeconds(10.0f * 60.0f);
    const float PurityBefore = Cell->TargetState.Meta.Purity;
    const float CorruptionBefore = Cell->TargetState.Meta.Corruption;

    Manager->UpdateEntityManifestations(1.0f);   // DeltaTime=1s -- удобная арифметика

    TestEqual(TEXT("Гнильники manifest on the corrupted Bog cell"), Cell->ManifestedEntityID, FName(TEXT("Гнильники")));
    TestTrue(TEXT("TargetState.Corruption nudged up"), Cell->TargetState.Meta.Corruption > CorruptionBefore);
    TestTrue(TEXT("TargetState.Purity nudged down"), Cell->TargetState.Meta.Purity < PurityBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_MokhovyeDukhiHealHighPurityTaiga,
    "Herbalist.AmbientEntity.MokhovyeDukhiHealHighPurityTaiga",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_MokhovyeDukhiHealHighPurityTaiga::RunTest(const FString& Parameters)
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
    Cell->State.Meta.Purity = 0.9f;   // выше порога 0.75
    const float PurityBefore = Cell->TargetState.Meta.Purity;
    const float StabilityBefore = Cell->TargetState.Meta.Stability;

    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Моховые духи manifest on the pristine Taiga cell"), Cell->ManifestedEntityID, FName(TEXT("Моховые духи")));
    TestTrue(TEXT("TargetState.Purity nudged up (единственный 'улучшающий' низший, §16.2)"), Cell->TargetState.Meta.Purity > PurityBefore);
    TestTrue(TEXT("TargetState.Stability nudged up"), Cell->TargetState.Meta.Stability > StabilityBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_StepnyeOgniOnlyManifestAtNight,
    "Herbalist.AmbientEntity.StepnyeOgniOnlyManifestAtNight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_StepnyeOgniOnlyManifestAtNight::RunTest(const FString& Parameters)
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
    Cell->Biome = EBiomeType::Steppe;
    Cell->bIsWater = false;

    // День (GameClockSeconds по умолчанию 0.0 -- рассвет) -- Степные огни не
    // должны проявляться, у их определения TriggerAxis::None, только ночь.
    Manager->SetGameClockSeconds(0.0f);
    TestFalse(TEXT("Sanity: it's day at t=0"), Manager->IsNight());
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Степные огни don't manifest during the day"), Cell->ManifestedEntityID, FName(TEXT("Степные огни")));

    // Ночь -- последние 6 игровых минут суток (32 по умолчанию), см. IsNight().
    Manager->SetGameClockSeconds(31.0f * 60.0f);
    TestTrue(TEXT("Sanity: it's night near the end of the day"), Manager->IsNight());
    const float DistortionBefore = Cell->TargetState.Meta.Distortion;
    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Степные огни manifest at night"), Cell->ManifestedEntityID, FName(TEXT("Степные огни")));
    TestTrue(TEXT("TargetState.Distortion nudged up (дезориентация)"), Cell->TargetState.Meta.Distortion > DistortionBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_NightHorrorAffectsEveryBiomeWithoutClaimingTheCell,
    "Herbalist.AmbientEntity.NightHorrorAffectsEveryBiomeWithoutClaimingTheCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_NightHorrorAffectsEveryBiomeWithoutClaimingTheCell::RunTest(const FString& Parameters)
{
    // §16.5: "сквозная ночная фаза" -- Вурдалаки/Навьи/... не привязаны к
    // биому (biome: Повсеместно) и не "владеют" клеткой как Гнильники
    // болотом; проверяем на биоме, где сегодня нет ни одного другого
    // определения (Тундра), чтобы эффект был виден изолированно.
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
    Cell->Biome = EBiomeType::Tundra;
    Cell->bIsWater = false;

    Manager->SetGameClockSeconds(0.0f);   // день
    const float DistortionBeforeDay = Cell->TargetState.Meta.Distortion;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("No change during the day on a biome with no other definition"),
        Cell->TargetState.Meta.Distortion, DistortionBeforeDay);

    Manager->SetGameClockSeconds(31.0f * 60.0f);   // ночь
    const float DistortionBeforeNight = Cell->TargetState.Meta.Distortion;
    const float CorruptionBeforeNight = Cell->TargetState.Meta.Corruption;
    Manager->UpdateEntityManifestations(1.0f);

    TestTrue(TEXT("TargetState.Distortion nudged up at night"), Cell->TargetState.Meta.Distortion > DistortionBeforeNight);
    TestTrue(TEXT("TargetState.Corruption nudged up at night"), Cell->TargetState.Meta.Corruption > CorruptionBeforeNight);
    TestEqual(TEXT("Night horror does not claim ManifestedEntityID -- it's atmosphere, not a 'owner'"),
        Cell->ManifestedEntityID, FName(NAME_None));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_KuvshinkinyDukhiRaiseResonanceAtNight,
    "Herbalist.AmbientEntity.KuvshinkinyDukhiRaiseResonanceAtNight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_KuvshinkinyDukhiRaiseResonanceAtNight::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Floodplain;
    Cell->bIsWater = false;   // заросли/берег -- земля, не вода Берегини

    Manager->SetGameClockSeconds(31.0f * 60.0f);   // ночь
    const float ResonanceBefore = Cell->TargetState.Meta.Resonance;
    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Кувшинкины духи manifest on the land edge of Floodplain at night"),
        Cell->ManifestedEntityID, FName(TEXT("Кувшинкины духи")));
    TestTrue(TEXT("TargetState.Resonance nudged up"), Cell->TargetState.Meta.Resonance > ResonanceBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_LedyanyeDukhiLowerMagnitudeInWinterOnly,
    "Herbalist.AmbientEntity.LedyanyeDukhiLowerMagnitudeInWinterOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_LedyanyeDukhiLowerMagnitudeInWinterOnly::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Tundra;
    Cell->bIsWater = false;
    Cell->TargetState.Magnitude = 0.5f;   // ненулевая точка отсчёта -- нудж вниз от 0.0 не отличим от клампа

    const float DayLengthSeconds = 32.0f * 60.0f;
    const float SeasonDurationSeconds = 117.0f * DayLengthSeconds;
    const float MidDaySeconds = 10.0f * 60.0f;   // фаза "День", не Рассвет/Закат/Полудница

    Manager->SetGameClockSeconds(MidDaySeconds);   // Весна
    const float MagnitudeInSpring = Cell->TargetState.Magnitude;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("No manifestation in Spring"), Cell->ManifestedEntityID, FName(NAME_None));
    TestEqual(TEXT("Magnitude untouched in Spring"), Cell->TargetState.Magnitude, MagnitudeInSpring);

    Manager->SetGameClockSeconds(SeasonDurationSeconds * 2.0f + MidDaySeconds);   // Зима
    const float MagnitudeBeforeWinter = Cell->TargetState.Magnitude;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Ледяные духи manifest on Tundra in Winter"), Cell->ManifestedEntityID, FName(TEXT("Ледяные духи")));
    TestTrue(TEXT("TargetState.Magnitude nudged down (freeze) in Winter"), Cell->TargetState.Magnitude < MagnitudeBeforeWinter);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_SukhoveykiAndStepnyeOgniShareSteppeCorrectly,
    "Herbalist.AmbientEntity.SukhoveykiAndStepnyeOgniShareSteppeCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_SukhoveykiAndStepnyeOgniShareSteppeCorrectly::RunTest(const FString& Parameters)
{
    // Регрессия на рефакторинг 2026-08-29: до него цикл прерывался на первом
    // совпадении Biome (Степные огни для Степи всегда шли первыми в реестре),
    // поэтому Суховейки -- второе определение на том же биоме -- никогда бы
    // не проверялись вовсе. Тест бьёт именно по дневному (не ночному) летнему
    // окну, где Степные огни заведомо не активны (нужна ночь), а Суховейки
    // должны сработать сами по себе.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Steppe;
    Cell->bIsWater = false;
    Cell->TargetState.Magnitude = 0.5f;

    const float DayLengthSeconds = 32.0f * 60.0f;
    const float SeasonDurationSeconds = 117.0f * DayLengthSeconds;
    const float MidDaySeconds = 10.0f * 60.0f;   // День, не ночь -- Степные огни не должны быть eligible

    Manager->SetGameClockSeconds(SeasonDurationSeconds * 1.0f + MidDaySeconds);   // Лето, День
    TestFalse(TEXT("Sanity: it's day, not night"), Manager->IsNight());

    const float MagnitudeBefore = Cell->TargetState.Magnitude;
    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Суховейки manifest on Steppe in Summer daylight (Степные огни needs night, not eligible)"),
        Cell->ManifestedEntityID, FName(TEXT("Суховейки")));
    TestTrue(TEXT("TargetState.Magnitude nudged down (иссушение) by Суховейки"), Cell->TargetState.Magnitude < MagnitudeBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_RusalkiOnlyHauntWaterAtNightNotLand,
    "Herbalist.AmbientEntity.RusalkiOnlyHauntWaterAtNightNotLand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_RusalkiOnlyHauntWaterAtNightNotLand::RunTest(const FString& Parameters)
{
    // Русалки (2026-08-29, решение пользователя): не "хозяин" §16.3 (нет
    // симметричного благословения за подношение -- враждебная сущность,
    // избегаемая опасность, не покровитель), а амбиентная зона §16.2,
    // bWaterOnly. Тест проверяет обе половины фильтра: не проявляются на
    // земляной кромке той же поймы (там Кувшинкины духи, bLandOnly -- не
    // должно быть путаницы между двумя bWaterOnly/bLandOnly определениями
    // на одном биоме) и не проявляются днём даже в воде.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* WaterCell = Manager->GetCell(0, 0);
    FGridCell* LandCell  = Manager->GetCell(1, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), WaterCell) || !TestNotNull(TEXT("Cell (1,0) exists"), LandCell))
    {
        Manager->Destroy();
        return false;
    }
    WaterCell->Biome = EBiomeType::Floodplain;
    WaterCell->bIsWater = true;
    LandCell->Biome = EBiomeType::Floodplain;
    LandCell->bIsWater = false;

    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("No Русалки in water by day"), WaterCell->ManifestedEntityID, FName(TEXT("Русалки")));

    Manager->SetGameClockSeconds(31.0f * 60.0f);   // Ночь
    const float DistortionBefore = WaterCell->TargetState.Meta.Distortion;
    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Русалки manifest in water at night"), WaterCell->ManifestedEntityID, FName(TEXT("Русалки")));
    TestTrue(TEXT("TargetState.Distortion nudged up in water"), WaterCell->TargetState.Meta.Distortion > DistortionBefore);
    TestNotEqual(TEXT("Русалки don't manifest on the land edge -- that's Кувшинкины духи's cell"),
        LandCell->ManifestedEntityID, FName(TEXT("Русалки")));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
