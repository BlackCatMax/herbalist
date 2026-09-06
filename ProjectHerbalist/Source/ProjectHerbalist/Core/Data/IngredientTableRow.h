// IngredientTableRow.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"   // EStorageContainerType, см. GrantsContainerType ниже
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

    // Класс актора этого растения (2026-09-03, PCG-расстановка) — пусто =
    // базовый AHerbalistResourceActor с мешем из ResourceMesh выше. Тот же
    // приём, что ActorClass у карточек бестиария. Нужен ровно затем, чтобы
    // отрастание собранного (StartRegeneration -> SpawnResourceActor)
    // воспроизводило ТО ЖЕ, что стояло: если растение поставил PCG-граф
    // своим Blueprint'ом, а отрастал бы жёстко базовый класс, куст после
    // первого сбора терял бы всё, что было в блюпринте.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    TSubclassOf<class AHerbalistResourceActor> ResourceActorClass;

    // ---- Высотный пояс произрастания (2026-09-03) ----
    // Прямой запрос: «нужна регулировка по высоте произрастания». Работает
    // как жёсткая маска с мягким краем, а НЕ как окна сезона/луны выше: те
    // намеренно никогда не обнуляют вес («не в сезон найти труднее, но
    // можно»), высота же — свойство места, а не момента. Выше границы леса
    // трава не растёт реже — она не растёт.
    //
    // В САНТИМЕТРАХ, не в метрах (переименовано 2026-09-03) — стандартная
    // единица расстояния UE (та же, что у Location.Z в Details), а не
    // отдельное правило только для этих трёх полей. Раньше называлось
    // "...Meters", и это привело к реальной путанице при заполнении
    // таблицы: значение ввели в сантиметрах по привычке (как для любого
    // другого расстояния в редакторе), а код делил высоту ландшафта на 100
    // и сравнивал её с "метрами" — пояс проверялся, но с числом в 100 раз
    // меньше нужного, и потому никогда не совпадал с реальной высотой
    // ландшафта. Имя поля теперь врёт в другую сторону было бы хуже, чем
    // отсутствие имени вовсе — так что оно называется тем, чем является.
    //
    // false (по умолчанию) — карточка растёт на любой высоте, поведение как
    // до появления полей.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth|Altitude")
    bool bUseAltitudeRange = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth|Altitude", meta = (EditCondition = "bUseAltitudeRange"))
    float MinAltitudeCentimeters = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth|Altitude", meta = (EditCondition = "bUseAltitudeRange"))
    float MaxAltitudeCentimeters = 100000.0f;   // 1000 м, тот же потолок, что был

    // Ширина полосы затухания у границ пояса: внутри пояса вес полный, за
    // его краем спадает линейно до нуля на этом расстоянии. 0 — резкая
    // граница. Нужна, чтобы пояс не выглядел вырезанным по линейке.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth|Altitude", meta = (EditCondition = "bUseAltitudeRange", ClampMin = "0.0"))
    float AltitudeFalloffCentimeters = 2500.0f;   // 25 м, тот же запас, что был

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    FRealState BaseState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    EIngredientClass Class = EIngredientClass::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    bool bIsWater = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TArray<EBiomeType> AllowedBiomes;

    // Водное растение (2026-09-02, прямой запрос пользователя: "если у
    // биома есть водные растения, то они разрешены к размещению на
    // поверхности воды, и вода одновременно доступна"). AllowedBiomes
    // выше остаётся тем же списком (какой земляной биом характерен для
    // этого растения — кувшинки и Речная пойма, например), этот флаг лишь
    // разрешает спавн НА клетках, которые сейчас вода (Cell.bIsWater),
    // вместо обычного запрета "на воде ресурсы не растут". Сбор воды
    // (CollectWater) и сбор этого растения (AHerbalistResourceActor) —
    // независимые механизмы, что заденет трейс игрока при клике, то и
    // сработает.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    bool bGrowsOnWater = false;

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
    // текстом карточек Плакун-травы/Чистотела, bDelicate — Медуницы.
    //
    // ПРОХОД ПО ВСЕМ 76 КАРТОЧКАМ ЗАВЕРШЁН 2026-09-04 (biome-за-biome, см.
    // CHANGELOG.md, 8 записей) -- каждая карточка построчно проверена на
    // мотив, "false"/"None" ниже означает "проверено, мотива нет", а не
    // "руки не дошли". Итог: bIronAverse/bDelicate новых мотивов не
    // нашли вовсе за пределами исходных трёх карточек -- оба флага
    // действительно редки в компендиуме. GardenNiche размечена у 41 из 76
    // карточек (ingredient_gathering_and_garden_flags.json) -- граница
    // "деревья/кустарники/ягодные кустарнички/продукт-с-дерева (кора) вне
    // 5 садовых ниш" установлена на Речной пойме и держалась консистентно
    // до последнего биома; яды глухих/гиблых мест (Белена/Мак/Аконит/
    // Белокрыльник) и нечёткие habitat-совпадения (Купальница, Типчак) --
    // тоже сознательно без ниши, не недосмотр.

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
    // в какой нише растёт нарочно у жилища — 41 из 76 карточек размечены
    // проходом 2026-08-31/2026-09-04 (см. комментарий у bIronAverse выше),
    // None у остальных 35 — деревья/кустарники/ягодные кустарнички/кора
    // (нет ниши для многолетнего куста-дерева-продукта), яды глухих мест,
    // нечёткие habitat-совпадения. AllowedBiomes выше остаётся
    // единственной правдой о том, где растение растёт В МИРЕ; это поле —
    // отдельный, параллельный список для клеток с постройкой-пристройкой,
    // не замена и не переопределение AllowedBiomes.
    //
    // 2026-09-04, шестая ниша Cave: Мухомор/Чага/Белый гриб переставлены
    // из Mycelium сюда -- та самая правка, которую GardenNiche-комментарий
    // выше (2026-08-31 §2.4) заранее просил ("развести грибы на два
    // подтипа пристройки" -- ящик на гнилой древесине для сапрофитов, свой
    // тёмный минеральный грот для микоризных/паразитических видов).
    // Кристаллы для будущей механики оберегов (ROADMAP.md) -- тоже Cave,
    // тем же жестом State->BaseState, что и травы.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Garden")
    EGardenNiche GardenNiche = EGardenNiche::None;

    // ---- Обереги (кристаллы Пещеры, DESIGN_Community_And_Homestead.md
    // §2.4, реализация 2026-09-04) — только КАТЕГОРИЯ эффекта живёт на
    // карточке, не сила/длительность: те, как и у остальных Zaryana/
    // Artifacts-таймеров этого проекта (InvisibilityCapDurationSeconds,
    // ShrineCoherenceBonus...), общие настройки на UHerbalistSettings
    // (WardDurationSeconds/WardBrewBoostCoherenceBonus/WardConcealmentRadius),
    // не число на каждой отдельной карточке — фольклор называет ХАРАКТЕР
    // защиты конкретного предмета, не её игровую силу в секундах/долях.
    //
    // false/None у всех обычных трав/грибов — только два кристалла первого
    // захода несут эти поля (Плакун-камень/Громовая стрела).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ward")
    bool bIsWard = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ward", meta = (EditCondition = "bIsWard"))
    EWardEffectType WardEffectType = EWardEffectType::None;

    // ---- Тиражные обереги (награда ритуалов перехода ярусов биомов,
    // RitualTypes.h::FRitualRecipeDefinition::GrantsIngredientID) — В
    // ОТЛИЧИЕ от трёх исходных кристаллов выше (Плакун-камень/Громовая
    // стрела/Куриный бог, bIsTieredWard=false у них, таймер по
    // WardDurationSeconds не тронут) у тиражных НЕТ ТАЙМЕРА ("как
    // активировал/надел оберег, так он и работает", прямой запрос) --
    // вместо срока действия их сила зависит от того, в "своём" ли биоме
    // используется эффект: WardHomeBiomes называет пару биомов яруса,
    // ДЛЯ КОТОРОГО предназначен конкретный кристалл (полная сила там),
    // вне них -- ослабленная (см. UHerbalistSettings::
    // TieredWardOutOfBiomeStrength, AGridWorldManager::ActivateTieredWard).
    // false/пусто у всех обычных трав/грибов и у трёх исходных кристаллов.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ward", meta = (EditCondition = "bIsWard"))
    bool bIsTieredWard = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ward", meta = (EditCondition = "bIsTieredWard"))
    TArray<EBiomeType> WardHomeBiomes;

    // ---- Инструменты сбора и оберег-при-сборе (DESIGN_Community_And_Homestead.md
    // §2.3, "полировка" 2026-09-06) — карточка становится физическим предметом
    // инвентаря вместо голого Exec-переключателя (AHerbalistPlayerController::
    // SetGatheringTool теперь резолвит владение по этому флагу, тем же приёмом,
    // что ActivateWard делает для bIsWard). Ось А (резак) — bIsGatheringTool +
    // GatheringToolType, три карточки (Железный/Медный серп, Костяной нож).
    // Ось Б (серебряный оберег) — bIsSilverWard, отдельно: не резак вообще, тот
    // же принцип раздельных осей, что уже у EGatheringTool (см. комментарий у
    // enum'а, HerbalistCoreTypes.h). false/None у всех обычных трав/грибов.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatheringTool")
    bool bIsGatheringTool = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatheringTool", meta = (EditCondition = "bIsGatheringTool"))
    EGatheringTool GatheringToolType = EGatheringTool::BareHands;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatheringTool")
    bool bIsSilverWard = false;

    // ---- Переносные контейнеры (Корзина/Мешок/Туёс, 2026-08-31/09-04,
    // "разберём тщательно" систему хранения, прямой запрос пользователя) ----
    // Если карточка представляет предмет-контейнер (утварь, не растение/
    // минерал/вода), здесь указано, какой EStorageContainerType личного
    // инвентаря игрока она даёт при экипировке (AHerbalistPlayerController::
    // EquipContainer -> UHerbalistInventoryComponent::TryEquipContainer).
    // None (по умолчанию, все обычные травы/грибы/минералы/обереги) —
    // карточка не является контейнером вовсе.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
    EStorageContainerType GrantsContainerType = EStorageContainerType::None;

    // ---- Сушка (DESIGN_Community_And_Homestead.md §2.2, "Хранилища" пункт
    // 3, 2026-09-04, прямой запрос пользователя: "хочу честно с изменением
    // свойств") — ДЕЛЬТА, не переопределение (FRealState DriedStateOverride
    // отброшен как менее честный вариант: сушка МЕНЯЕТ уже собранное
    // растение со всем его собственным разбросом состояния — биом сбора,
    // порча к моменту сушки, случайный джиттер MergeStack — не стирает его
    // до нового фиксированного значения). Складывается с Item.State.Meta в
    // момент завершения сушки (UHerbalistInventoryComponent::TickComponent,
    // тем же приёмом клампа [0,1], что уже ApplyDecayToItem), Direction не
    // трогается — ось "характера" (Body/Mind/Spirit/Nature) сушка физически
    // не меняет, только реальные Meta-оси (сила/чистота/испорченность),
    // см. довод у каждой отдельной карточки (раздел "## Сушка" компендиума).
    //
    // Все нули (дефолт) — сушка НЕ меняет алхимические оси этой карточки,
    // только сохранность (bIsDried уже даёт свой decay-множитель отдельно
    // от этого поля, DriedItemDecayMultiplier, HerbalistSettings.h). Это
    // честный, распространённый случай реальной травнической практики —
    // большинство трав компендиума сохраняют профиль при сушке, просто
    // портятся медленнее; не выдумываем дельту без реального ботанического
    // основания (тот же принцип "честные пробелы", что уже у AllowedBiomes/
    // GardenNiche прохода 2026-09-04, см. CHANGELOG.md). Ненулевые дельты
    // проставлены точечно (DryingStatePatchCommandlet) только 6 карточкам
    // из 76 Plant/Fungus, с реальным ботаническим обоснованием каждой.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drying")
    FMeta DriedStateDelta;

    // Длительность сушки ЭТОЙ карточки, в секундах (2026-09-05, прямой запрос
    // пользователя: "процесс сушки у разных растений разный (длительность)").
    // До этой правки UHerbalistSettings::DryingDurationSeconds было ОДНИМ
    // глобальным числом на все 76 карточек Plant/Fungus разом — тонкий лист
    // и плотный корень/кора сохли одинаково, что противоречит реальной
    // травнической практике (лист/цветок сохнет за день, корень/кора — за
    // недели). См. довод и калибровку по категориям (Трава/Ягода/Гриб/
    // плотный материал) у UHerbalistSettings::DryingDurationSeconds.
    //
    // Сентинел -1 (дефолт) -- "эта карточка НЕ несёт явного значения, читать
    // глобальный фолбэк" (UHerbalistInventoryComponent::TickComponent). НЕ 0
    // сознательно: карточка, для которой заход забудет проставить число (или
    // будущая новая карточка компендиума), должна упасть на разумный
    // глобальный фолбэк, а не сохнуть мгновенно/никогда из-за забытого нуля.
    // Все 76 карточек Plant/Fungus пропатчены точечно (см.
    // ingredient_drying_duration_patch.json, IngredientDryingDurationPatchCommandlet)
    // -- сентинел остаётся только защитой на будущее, не рабочим состоянием
    // сейчас.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drying")
    float DryingDurationSeconds = -1.0f;
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

    // Высота клетки над нулём мира, В САНТИМЕТРАХ (2026-09-03, переименовано
    // из AltitudeMeters -- см. подробный довод у FIngredientTableRow::
    // MinAltitudeCentimeters). Заполняется менеджером НАПРЯМУЮ из
    // закэшированной высоты ландшафта (GetCellHeight уже в см, конвертация
    // не нужна вовсе); 0 = «высота неизвестна или неважна», и тогда
    // высотный гейт карточки не применяется вовсе (см.
    // FIngredientTableRow::bUseAltitudeRange).
    float AltitudeCentimeters = 0.0f;
    bool bAltitudeKnown = false;
};