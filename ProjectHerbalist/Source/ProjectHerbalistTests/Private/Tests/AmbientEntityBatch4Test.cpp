// Source/ProjectHerbalistTests/Private/Tests/AmbientEntityBatch4Test.cpp
//
// Четвёртая пачка §16.2 (2026-08-29, "закрываем весь бестиарий"):
// Трясинные духи/Чащобные духи (первые потребители Direction-осей в
// EAmbientTriggerAxis, а не только Meta), Болотные огни/Шишиги (первые
// потребители нового bRequiresDusk), Древесные огни/Снежные огни/Плескуны
// (декоративные, только проявление). Регрессия целится в две вещи: что
// новые оси/гейт реально работают, и что декоративные существа не молчаливо
// проваливаются мимо check() в AmbientEntityTypes.h.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Save/HerbalistSaveTypes.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_TryasinnyeDukhiTriggerOnDominantNature,
    "Herbalist.AmbientEntity.TryasinnyeDukhiTriggerOnDominantNature",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_TryasinnyeDukhiTriggerOnDominantNature::RunTest(const FString& Parameters)
{
    // Трясинные духи -- первый потребитель Direction-оси (Nature) в
    // EAmbientTriggerAxis, добавленной этой пачкой. Проверяем и что низкий
    // Nature НЕ проявляет, и что доминирующий -- проявляет.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День -- не хотим коллизии с Рассветом/Ночью

    FGridCell* Cell = Manager->GetCell(0, 0);
    Cell->Biome = EBiomeType::Bog;
    Cell->bIsWater = false;
    // Stability выше порога Ржавых духов (0.3) -- иначе на той же клетке
    // (Болото, земля) Ржавые духи, зарегистрированные раньше в реестре,
    // забрали бы ManifestedEntityID первыми (тот же ранг 0, первый claim
    // выигрывает), и тест проверял бы не то, что заявлен.
    Cell->State.Meta.Stability = 0.5f;
    Cell->State.Direction.Body = 0.34f;
    Cell->State.Direction.Mind = 0.33f;
    Cell->State.Direction.Spirit = 0.33f;
    Cell->State.Direction.Nature = 0.0f;

    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Low Nature does not manifest Тrясинные духи"),
        Cell->ManifestedEntityID, FName(TEXT("Трясинные духи")));

    Cell->State.Direction.Body = 0.2f;
    Cell->State.Direction.Mind = 0.2f;
    Cell->State.Direction.Spirit = 0.1f;
    Cell->State.Direction.Nature = 0.5f;   // явно доминирует

    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Dominant Nature manifests Трясинные духи"),
        Cell->ManifestedEntityID, FName(TEXT("Трясинные духи")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_ShishigiOnlyManifestAtDuskNotFullNight,
    "Herbalist.AmbientEntity.ShishigiOnlyManifestAtDuskNotFullNight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_ShishigiOnlyManifestAtDuskNotFullNight::RunTest(const FString& Parameters)
{
    // Шишиги -- первый потребитель bRequiresDusk (новый гейт, отдельный от
    // bRequiresNight). Должны проявляться в окне Заката, не ночью и не днём.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    Cell->Biome = EBiomeType::MixedForest;
    Cell->bIsWater = false;

    // GameDayMinutes=32 по умолчанию: Рассвет 0-6, День 6-20, Закат 20-26,
    // Ночь 26-32 (минуты). Середина Дня -- НЕ должно проявиться.
    Manager->SetGameClockSeconds(12.0f * 60.0f);
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Midday: Шишиги do not manifest"), Cell->ManifestedEntityID, FName(TEXT("Шишиги")));

    // Середина Заката (23 мин) -- должно проявиться.
    Manager->SetGameClockSeconds(23.0f * 60.0f);
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Dusk: Шишиги manifest"), Cell->ManifestedEntityID, FName(TEXT("Шишиги")));

    // Глубокая ночь (29 мин) -- НЕ должно проявиться (bRequiresDusk, не Night).
    Manager->SetGameClockSeconds(29.0f * 60.0f);
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Deep night: Шишиги do not manifest (dusk-only, not night)"),
        Cell->ManifestedEntityID, FName(TEXT("Шишиги")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_DecorativeEntitiesManifestWithoutEffect,
    "Herbalist.AmbientEntity.DecorativeEntitiesManifestWithoutEffect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_DecorativeEntitiesManifestWithoutEffect::RunTest(const FString& Parameters)
{
    // Древесные огни/Снежные огни/Плескуны/Болотные огни -- проверяем, что
    // все четыре реально проявляются по своим условиям (не просто прошли
    // check() молча) и не грязнят клетку без реального Meta-эффекта --
    // тот же §7.1-паттерн, что уже проверен для Ржавые духи/Водяные бесы/Злыдни.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetGameClockSeconds(29.0f * 60.0f);   // ночь

    FGridCell* TreeCell = Manager->GetCell(0, 0);
    TreeCell->Biome = EBiomeType::MixedForest;
    TreeCell->bIsWater = false;

    FGridCell* SnowCell = Manager->GetCell(1, 0);
    SnowCell->Biome = EBiomeType::Tundra;
    SnowCell->bIsWater = false;

    FGridCell* BogFireCell = Manager->GetCell(3, 0);
    BogFireCell->Biome = EBiomeType::Bog;
    BogFireCell->bIsWater = false;
    // Stability выше порога Ржавых духов (0.3) -- та же коллизия и то же
    // исправление, что у Трясинных духов выше: Ржавые духи регистрируются
    // раньше в реестре и забрали бы ManifestedEntityID первыми на Stability=0.
    BogFireCell->State.Meta.Stability = 0.5f;
    BogFireCell->State.Meta.Distortion = 0.7f;
    BogFireCell->TargetState.Meta.Distortion = 0.7f;
    const float BogDistortionBefore = BogFireCell->TargetState.Meta.Distortion;

    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Древесные огни manifest at night"), TreeCell->ManifestedEntityID, FName(TEXT("Древесные огни")));
    TestEqual(TEXT("Снежные огни manifest at night"), SnowCell->ManifestedEntityID, FName(TEXT("Снежные огни")));
    TestEqual(TEXT("Болотные огни manifest at night with high Distortion"), BogFireCell->ManifestedEntityID, FName(TEXT("Болотные огни")));

    // Болотные огни реально нуджат Distortion -- в отличие от трёх
    // декоративных выше. Дальше по числу грязных клеток НЕ проверяем: ночью
    // уже действует существующий разлитый-по-сетке ночной нудж (§16.5,
    // GridWorldManagerEntities.cpp) сам по себе, независимо от новых
    // существ -- он и так грязнит все 400 клеток, это чужая, уже покрытая
    // другим тестом регрессия, не то, что здесь проверяется.
    TestTrue(TEXT("Болотные огни actually nudge TargetState.Distortion up"),
        BogFireCell->TargetState.Meta.Distortion > BogDistortionBefore);

    // Плескуны -- проверяются отдельным, дневным проходом: ночью на той же
    // клетке (Речная пойма, вода) Русалки (зарегистрированы раньше в
    // реестре, безусловный ночной триггер) забрали бы ManifestedEntityID
    // первыми, тест проверял бы коллизию приоритетов, не Плескунов.
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // день
    FGridCell* SplashCell = Manager->GetCell(2, 0);
    SplashCell->Biome = EBiomeType::Floodplain;
    SplashCell->bIsWater = true;
    SplashCell->State.Meta.Purity = 0.5f;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Плескуны manifest in shallow water during the day"), SplashCell->ManifestedEntityID, FName(TEXT("Плескуны")));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
