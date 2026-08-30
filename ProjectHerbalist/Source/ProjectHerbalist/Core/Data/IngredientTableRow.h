// IngredientTableRow.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "IngredientTableRow.generated.h"

UENUM(BlueprintType)
enum class EIngredientClass : uint8
{
    Water       UMETA(DisplayName = "Water"),
    Plant       UMETA(DisplayName = "Plant"),
    Mineral     UMETA(DisplayName = "Mineral"),
    Fungus      UMETA(DisplayName = "Fungus"),
    Catalyst    UMETA(DisplayName = "Catalyst"),
    Essence     UMETA(DisplayName = "Essence"),
    Unknown     UMETA(DisplayName = "Unknown")
};

// Окно времени суток для сбора (DESIGN_World_State.md §15/§16, "ПогодноеОкно"
// сестра — "ВременноеОкно"), 2026-08-29. День = Рассвет/Закат/Ночь оба false
// (GridWorldManager::IsDawn/IsDusk/IsNight уже взаимоисключающие и покрывают
// сутки целиком, см. 15_Cycles_And_Shrines §15.2) — не нужен отдельный
// IsDay(), просто отсутствие остальных трёх.
UENUM(BlueprintType)
enum class EHarvestTimeWindow : uint8
{
    Any     UMETA(DisplayName = "Любое время"),
    Dawn    UMETA(DisplayName = "Рассвет"),
    Day     UMETA(DisplayName = "День"),
    Dusk    UMETA(DisplayName = "Закат"),
    Night   UMETA(DisplayName = "Ночь")
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FIngredientTableRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UStaticMesh* ResourceMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    FRealState BaseState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    EIngredientClass Class = EIngredientClass::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    bool bIsWater = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TArray<EBiomeType> AllowedBiomes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "1"))
    int32 RarityWeight = 1;

    // Множитель скорости порчи в инвентаре (1.0 = стандартная скорость)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float DecayRate = 1.0f;

    // Сопротивляемость месту при сборе: насколько трава держит свою природу
    // вопреки биому. 0 = полностью принимает характер места (эффективный вес
    // биома = HarvestBiomeWeight), 1 = не поддаётся вовсе (собирается ровно
    // своим BaseState). Сильные/своевольные травы фольклора — ближе к 1.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Resilience = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    FName Element;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    TArray<FName> Tags;

    // ---- Окна сбора (DESIGN_World_State.md §15/§16, звено 8: "Сезон/погода
    // → ингредиенты"), 2026-08-29. Тот же язык гейтов, что уже устоялся в
    // AmbientEntityTypes.h (bRequiresX/RequiredX, пусто = без ограничения) —
    // не hard-фильтр (в отличие от AllowedBiomes, где пустой список значит
    // "нигде не растёт"), а мягкий множитель в GetRandomResourceForBiome
    // (IngredientWindowMismatchMultiplier, HerbalistSettings.h): вне окна
    // шанс резко падает, но не запирается в 0.

    // Пусто = любой сезон. Небольшая проверенная свобода (не строгий гейт по
    // месяцу) — в компендиуме сроки цветения/сбора почти всегда указаны
    // диапазоном ("май-июнь"), а проект осознанно держит только 3 сезона
    // (см. bAutumnOnly ниже и комментарий у GridWorldManager::IsLateSummer).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Window")
    TArray<ESeason> AllowedSeasons;

    // Второй, более узкий гейт ВНУТРИ Лета — прокси "осени" (см. bRequiresLateSummer
    // в AmbientEntityTypes.h, тот же принцип, тот же IsLateSummer()). Применяется
    // ТОЛЬКО когда текущий сезон Лето: если AllowedSeasons также включает Весну,
    // весенний сбор им не затрагивается — типичный случай компендиума "корень
    // копают ранней весной ИЛИ поздней осенью" (два отдельных окна, не одно).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Window")
    bool bAutumnOnly = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Window")
    EHarvestTimeWindow HarvestTimeWindow = EHarvestTimeWindow::Any;

    // Фаза луны (GridWorldManager::GetMoonPhase, §15.3) — компендиум называет
    // её примерно в половине карточек как самостоятельное, не завязанное на
    // время суток условие ("собирать на убывающую луну").
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Window")
    bool bRequiresMoonPhase = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Window")
    EMoonPhase RequiredMoonPhase = EMoonPhase::NewMoon;

    // "Сухая погода" — самое частое погодное условие компендиума ("собирать в
    // сухой день", грозы явно запрещены). GridWorldManager::IsRainy()/IsBlizzard()
    // читаются как "не сухо" — оба означают осадки, а не только дождь.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Window")
    bool bRequiresDryWeather = false;

    // ---- Инструмент сбора (DESIGN_Community_And_Homestead.md §2.3, флаги
    // найдены проходом по компендиуму 2026-08-31 — bIronAverse подтверждён
    // текстом карточек Плакун-травы/Чистотела, bDelicate — Медуницы; для
    // остальных 73 карточек оба флага пока false, не проверено построчно
    // "нет мотива" не то же самое, что "проверено и мотива нет").

    // Трава чует железо — собранная железным инструментом теряет силу
    // (см. ToolQualityMultiplier в PipelineV2.cpp). Голые руки/медь/кость
    // одинаково безопасны.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Window")
    bool bIronAverse = false;

    // Тонкая трава — костяной нож при срезе сохраняет её силу лучше любого
    // другого инструмента (бонус, не отдельный гейт).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Window")
    bool bDelicate = false;

    // Пристройка сада (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31),
    // в какой нише растёт нарочно у жилища — None для большинства (деревья,
    // ещё не размеченные 66 из 76 карточек). AllowedBiomes выше остаётся
    // единственной правдой о том, где растение растёт В МИРЕ; это поле —
    // отдельный, параллельный список для клеток с постройкой-пристройкой,
    // не замена и не переопределение AllowedBiomes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Garden")
    EGardenNiche GardenNiche = EGardenNiche::None;
};

// Текущие условия сбора в момент вызова GetRandomResourceForBiome — читаются
// один раз в AGridWorldManager::SpawnResourcesInCell (та же клетка для всех
// 1-3 ресурсов, спавнящихся в неё) и сравниваются с окнами выше построчно.
// Отдельная структура, а не россыпь bool/enum параметров — те же 5 сигналов
// (сезон/окно суток/луна/погода) уже читает GetEntityManifestationPriority
// окольным путём через сам AGridWorldManager, здесь просто собраны в одно
// значение для передачи через границу GameInstanceSubsystem.
USTRUCT()
struct FHarvestContext
{
    GENERATED_BODY()

    ESeason Season = ESeason::Spring;
    bool bLateSummer = false;
    EHarvestTimeWindow TimeOfDay = EHarvestTimeWindow::Day;
    EMoonPhase MoonPhase = EMoonPhase::NewMoon;
    bool bDryWeather = true;
};