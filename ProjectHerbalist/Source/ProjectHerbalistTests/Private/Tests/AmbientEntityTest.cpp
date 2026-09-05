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
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_GnilnikiRespectsGlobalHysteresisSetting,
    "Herbalist.AmbientEntity.GnilnikiRespectsGlobalHysteresisSetting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_GnilnikiRespectsGlobalHysteresisSetting::RunTest(const FString& Parameters)
{
    // Аудит 2026-09-05: Низший ранг (Гнильники и другие в AmbientEntityTypes.h)
    // раньше читал СВОЙ отдельный Def.HysteresisMargin, а не общую
    // UHerbalistSettings::EntityManifestationHysteresis, хотя её собственный
    // комментарий прямо называет "Corruption у Гнильников" в числе
    // потребителей. Оба дефолта совпадают (0.05f), поэтому баг был не виден,
    // пока настройку не поменяли бы -- этот тест её меняет и проверяет, что
    // Гнильники это чувствуют.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Bog;
    Cell->bIsWater = false;
    Cell->State.Meta.Corruption = 0.62f;   // выше порога 0.6, но не выше 0.6 + дефолтный запас 0.05
    // Ржавые духи (тоже Bog, триггер -- низкая Stability) иначе тоже
    // становятся eligible при обнулении общего запаса ниже и перехватывают
    // клетку первыми по порядку реестра -- изолируемся от них высокой
    // Stability, тот же приём, что уже применяют другие тесты этого файла
    // для смежных Ambient-определений одного биома.
    Cell->State.Meta.Stability = 1.0f;
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // середина Дня, та же изоляция от Рассвета, что и выше

    UHerbalistSettings* Settings = GetMutableDefault<UHerbalistSettings>();
    const float SavedMargin = Settings->EntityManifestationHysteresis;

    Settings->EntityManifestationHysteresis = 0.1f;   // 0.62 < 0.6+0.1=0.7 -- не должно сработать
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Большой общий запас гистерезиса подавляет вход Гнильников"),
        Cell->ManifestedEntityID, FName(TEXT("Гнильники")));

    Settings->EntityManifestationHysteresis = 0.0f;   // 0.62 > 0.6+0=0.6 -- теперь должно сработать
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Нулевой общий запас гистерезиса пропускает Гнильников -- настройка реально читается"),
        Cell->ManifestedEntityID, FName(TEXT("Гнильники")));

    Settings->EntityManifestationHysteresis = SavedMargin;
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
    // болотом; проверяем на биоме, где условие Низшего не завязано на само
    // состояние клетки (Лесостепь -- только Межевые, 2026-08-29, триггер по
    // соседям в сетке, не по Meta/HarvestStress), чтобы эффект был виден
    // изолированно. Соседей (0,0) явно ставим тем же биомом -- иначе
    // Межевые срабатывают на границе с чем угодно другим на процедурной сетке.
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
    Cell->Biome = EBiomeType::ForestSteppe;
    Cell->bIsWater = false;
    if (FGridCell* Right = Manager->GetCell(1, 0)) { Right->Biome = EBiomeType::ForestSteppe; }
    if (FGridCell* Down  = Manager->GetCell(0, 1)) { Down->Biome  = EBiomeType::ForestSteppe; }

    Manager->SetGameClockSeconds(0.0f);   // день
    const float DistortionBeforeDay = Cell->TargetState.Meta.Distortion;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("No change during the day on a biome with no other definition"),
        Cell->TargetState.Meta.Distortion, DistortionBeforeDay);

    Manager->SetGameClockSeconds(31.0f * 60.0f);   // ночь
    const float DistortionBeforeNight = Cell->TargetState.Meta.Distortion;
    const float CorruptionBeforeNight = Cell->TargetState.Meta.Corruption;
    const float SpiritBeforeNight = Cell->TargetState.Direction.Spirit;
    Manager->UpdateEntityManifestations(1.0f);

    TestTrue(TEXT("TargetState.Distortion nudged up at night"), Cell->TargetState.Meta.Distortion > DistortionBeforeNight);
    TestTrue(TEXT("TargetState.Corruption nudged up at night"), Cell->TargetState.Meta.Corruption > CorruptionBeforeNight);
    // §15.2, третья часть строки Ночи ("усиление оси Spirit в Direction",
    // Tier 1 п.1.1, 2026-09-02) -- та же клетка, тот же нудж-блок IsNight(),
    // раньше проверялись только Meta-поля.
    TestTrue(TEXT("TargetState.Direction.Spirit nudged up at night"), Cell->TargetState.Direction.Spirit > SpiritBeforeNight);
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

    // Ночь, но НЕ Новолуние (17060с -- Ночь того же дня, но день 8 цикла,
    // внутри Растущей [13440,26880) -- иначе коллизия с Омутными огнями,
    // 2026-08-29: тот же биом+вода+ночь, но ещё и Новолуние, зарегистрированы
    // раньше специально ради этого более узкого условия, см. AmbientEntityTypes.h).
    Manager->SetGameClockSeconds(17060.0f);
    const float DistortionBefore = WaterCell->TargetState.Meta.Distortion;
    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Русалки manifest in water at night"), WaterCell->ManifestedEntityID, FName(TEXT("Русалки")));
    TestTrue(TEXT("TargetState.Distortion nudged up in water"), WaterCell->TargetState.Meta.Distortion > DistortionBefore);
    TestNotEqual(TEXT("Русалки don't manifest on the land edge -- that's Кувшинкины духи's cell"),
        LandCell->ManifestedEntityID, FName(TEXT("Русалки")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_ItemCorruptingEntitiesManifestWithoutDirtyingTheCell,
    "Herbalist.AmbientEntity.ItemCorruptingEntitiesManifestWithoutDirtyingTheCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_ItemCorruptingEntitiesManifestWithoutDirtyingTheCell::RunTest(const FString& Parameters)
{
    // Ржавые духи/Водяные бесы/Злыдни (§16.2 "порча инструмента"/"мелкая
    // порча снаряжения") -- изначально задумывались с единственным эффектом
    // ItemCorruptionRate, читаемым напрямую HerbalistInventoryComponent, но
    // это отменено правкой пользователя 2026-08-29 ("травы портятся сами по
    // себе... на сохранность влияют сами контейнеры хранения" --
    // EStorageContainerType в HerbalistInventoryComponent.h). Сейчас у всех
    // троих вообще нет Meta/Direction-эффекта -- они просто манифестируются.
    // Тест по-прежнему ценен: проверяем и что они манифестируют по своим
    // условиям, и что при этом НЕ попадают в Delta.TargetStateNudges --
    // это ровно тот класс бага (bChanged безусловно true даже без реального
    // изменения), который уже чинили для ApplyBiomeInfluences/ночного нуджа
    // (AUDIT_AND_REFACTORING_PLAN.md §7.1) и который здесь можно было бы
    // повторить по новой, добавляя существ без Meta/Direction-эффекта.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // GameClockSeconds по умолчанию 0.0 -- фаза Рассвет (§15.2), которая
    // сама разливает Purity/Stability-нудж по ВСЕЙ сетке и грязнила бы все
    // 400 клеток независимо от трёх тестовых -- та же ловушка, что уже
    // чинилась для GnilnikiStillManifestsAfterRefactor. Явно ставим середину Дня.
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    // Ржавые духи: Болото, земля, Stability < 0.3.
    FGridCell* RustCell = Manager->GetCell(0, 0);
    RustCell->Biome = EBiomeType::Bog;
    RustCell->bIsWater = false;
    RustCell->State.Meta.Stability = 0.1f;

    // Водяные бесы: Речная пойма, вода, Distortion > 0.5.
    FGridCell* MurkyCell = Manager->GetCell(1, 0);
    MurkyCell->Biome = EBiomeType::Floodplain;
    MurkyCell->bIsWater = true;
    MurkyCell->State.Meta.Distortion = 0.7f;

    // Злыдни: Широколиств. лес, земля, HarvestStress > 0.6.
    FGridCell* NeglectedCell = Manager->GetCell(2, 0);
    NeglectedCell->Biome = EBiomeType::BroadleafForest;
    NeglectedCell->bIsWater = false;
    NeglectedCell->HarvestStress = 0.9f;

    const FRealState RustTargetBefore = RustCell->TargetState;
    const FRealState MurkyTargetBefore = MurkyCell->TargetState;
    const FRealState NeglectedTargetBefore = NeglectedCell->TargetState;

    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Ржавые духи manifest on low-Stability Bog"), RustCell->ManifestedEntityID, FName(TEXT("Ржавые духи")));
    TestEqual(TEXT("Водяные бесы manifest on high-Distortion water"), MurkyCell->ManifestedEntityID, FName(TEXT("Водяные бесы")));
    TestEqual(TEXT("Злыдни manifest on high-HarvestStress cell"), NeglectedCell->ManifestedEntityID, FName(TEXT("Злыдни")));

    // Раньше проверялось глобально (CaptureSaveCells().Num()==0) -- перестало
    // быть верно 2026-08-29 с добавлением Межевых (Лесостепь, реальный
    // Nature-эффект на границах биомов): на процедурно сгенерированной сетке
    // 20x20 почти наверняка есть граничные клетки Лесостепи где-то ещё в тех
    // же 400 -- и это ожидаемо, не баг. Проверяем прицельно: у ЭТИХ ТРЁХ
    // клеток (Ржавые духи/Водяные бесы/Злыдни, все без реального эффекта)
    // TargetState не изменился вовсе -- то самое §7.1, просто не глобальным
    // счётчиком, а точечно.
    TestEqual(TEXT("Ржавые духи cell TargetState unchanged (no real effect)"),
        RustCell->TargetState.Meta.Stability, RustTargetBefore.Meta.Stability);
    TestEqual(TEXT("Водяные бесы cell TargetState unchanged (no real effect)"),
        MurkyCell->TargetState.Meta.Distortion, MurkyTargetBefore.Meta.Distortion);
    TestEqual(TEXT("Злыдни cell TargetState unchanged (no real effect)"),
        NeglectedCell->TargetState.Meta.Corruption, NeglectedTargetBefore.Meta.Corruption);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
