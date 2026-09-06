// HerbalistCoreTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"
#include "HerbalistCoreTypes.generated.h"

// ========== Enum'ы ==========

// Лунный цикл (02_GDD/15_Cycles_And_Shrines.md §15.3): 4 фазы по 7 игровых
// суток = 28-суточный месяц, тот же принцип "круг из 4 фаз", что уже есть
// у суточного цикла (Рассвет/День/Закат/Ночь), но в масштабе месяца, а не
// дня — см. §15.1 "три круга — одна структура".
UENUM(BlueprintType)
enum class EMoonPhase : uint8
{
    NewMoon,      // Новолуние
    WaxingMoon,   // Растущая
    FullMoon,     // Полнолуние
    WaningMoon    // Убывающая
};

// Годовой круг (02_GDD/15_Cycles_And_Shrines.md §15.4): три сезона, не
// четыре — трёхполье (яровое/озимое/пар), а не условное деление года на
// равные четверти. Тот же принцип "круг из N фаз", что у суток (4) и луны
// (4), просто N=3 здесь — сознательно, не упрощение.
UENUM(BlueprintType)
enum class ESeason : uint8
{
    Spring,   // Весна
    Summer,   // Лето
    Winter    // Зима
};

// Три исхода у Буяна (02_GDD/18_Ending.md §18.1, 2026-09-01) — выбор через
// действие (три разных Exec-команды на контроллере), не диалоговое меню
// (§18.1: "сломало бы уже установленный принцип «характер через реакцию
// мира, не через реплики»"). Финальные тексты/сцены НЕ реализованы —
// лорная задача 22_Lore_Roadmap.md, здесь только ветвление и заглушки.
// Живёт в HerbalistCoreTypes.h, не GridWorldManager.h (где его заводит
// план шага 7) — тот же общий дом малых enum'ов, что уже EMoonPhase/ESeason
// выше, нужен и HerbalistSaveTypes.h без затягивания всего GridWorldManager.h.
UENUM(BlueprintType)
enum class EBuyanPath : uint8
{
    None,
    Guardian,       // Путь 1 — пожертвовать собой, стать стражем Буяна
    TradePlaces,    // Путь 2 — обмануть смерть, поменяться местами с Заряной
    AcceptReality   // Путь 3 — принять реальность как есть
};

UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    Tundra,
    Taiga,
    MixedForest,
    BroadleafForest,
    ForestSteppe,
    Steppe,
    Floodplain,
    Bog
};

// Пристройка сада (DESIGN_Community_And_Homestead.md §2.4, реализация
// 2026-08-31) — какую нишу (не биом целиком) физически подделывает
// постройка у жилища. None = обычный ингредиент, ни в одной пристройке не
// растёт нарочно (в первую очередь деревья — полноразмерный лес не
// пересаживается в грядку, структурный предел, не открытый вопрос).
UENUM(BlueprintType)
enum class EGardenNiche : uint8
{
    None,
    Mycelium,    // Грибница — тёмный влажный ящик на гнилой древесине (сапрофиты)
    RootCellar,  // Погреб/ледник — холод, темнота
    Pond,        // Водоём — кадка/маленький пруд
    SunnyBed,    // Открытая грядка на солнце
    ShadeBed,    // Тенистая грядка подлеска
    // Шестая, 2026-09-04 (прямой запрос пользователя, DESIGN_Community_And_
    // Homestead.md §2.4) — тёмный минеральный грот. Забирает у Грибницы её
    // же не подходивший ей случай: микоризные/паразитические грибы (нужен
    // живой корень-хозяин, не гнилая деревяшка в ящике) и вводит кристаллы
    // (растут тем же жестом State->BaseState, что и травы — задел под
    // будущую механику оберегов, ROADMAP.md).
    Cave
};

// Механика оберегов (кристаллы Пещеры, DESIGN_Community_And_Homestead.md
// §2.4, реализация 2026-09-04) — какой из "слабый, но непрерывный" эффектов
// даёт активированный оберег. Прямой запрос: "работа оберегов слабая, но
// постоянная, есть временные лимиты, не постоянное действие" — НЕ разовый
// расходуемый предмет (как Гребень/Яблоко) и НЕ бессрочный баф при ношении,
// а активация с GameClockSeconds-экспаери, тем же паттерном, что уже
// InvisibilityCapExpiryGameSeconds/YouthAppleClarityBoostExpiryGameSeconds/
// AlkonostSuppressionExpiryGameSeconds (GridWorldManager.h) — выбран из трёх
// предложенных кандидатов таймера именно он: DisappearTimerHandle
// (AHerbalistResourceActor) — разовый таймер НА АКТОР, не подходит состоянию
// игрока; ShrineNeglectDecayDays/HistoryPurityLerpRate — плавное затухание
// БЕЗ жёсткого нуля, а тут нужен настоящий "включено, потом выключено".
//
// Три эффекта реализованы к 2026-09-04 (второй заход, вертикальный срез
// как и первый): BrewBoost (Громовая стрела), EntityConceal (Плакун-камень)
// и MorokReduction (Куриный бог — камень с естественной сквозной дырой,
// настоящий народный оберег: вешался над постелью/в курятнике против
// кикиморы, порчи и дурных снов, см. компендиумную карточку). PathReveal
// (показ троп) остаётся СОЗНАТЕЛЬНО не заведён — второй заход (2026-09-04)
// нашёл реальный фольклорный мотив "камня на распутье" (бел-горюч
// Латырь-камень трёх дорог, былина об Илье Муромце: "Направо пойдёшь —
// ..."), но это ЕДИНСТВЕННЫЙ мифический камень-граница миров в начале ВСЕХ
// дорог, а не тип находимого/собираемого камня вроде плакун-камня
// (реальная банная/купальская практика) или громовой стрелы (реальный
// амулет-белемнит, хранившийся в каждом доме) — под механику "вырастить в
// Пещере, носить, активировать" эту находку тянуть значило бы придумывать
// портативную форму мифа, которой в источниках нет. Народное средство от
// морока/кружения лешего в лесу тоже реально существует (вывернуть одежду
// наизнанку, переобуться) — но это не камень вовсе, другой класс предмета.
// Добавлять значение без честной опоры значило бы имитировать реализацию.
// См. ROADMAP.md/CHANGELOG.md.
UENUM(BlueprintType)
enum class EWardEffectType : uint8
{
    None,
    // Слабая непрерывная надбавка к Coherence на варке, пока оберег активен
    // — массовый/крафтящийся аналог Камня-оберега (§21.3), но не разовое
    // спасение неудачного Bifurcation-броска, а постоянная маленькая
    // добавка, тем же путём, что уже даёт бонус капища к Coherence
    // (ShrineCoherenceBonus, PipelineV2.cpp::ProcessApplyCommand).
    BrewBoost,
    // Слабая версия Шапки-невидимки — подавляет НОВЫЕ проявления сущностей
    // в маленьком радиусе вокруг игрока, пока оберег активен (WardConcealmentRadius,
    // заметно меньше InvisibilityCapRadius — "слабый", не полноценный артефакт).
    EntityConceal,
    // Куриный бог — снижает воспринятое искажение (ComputePerceptionDistortion,
    // "Морок") в маленьком радиусе вокруг места активации (тот же Center+Radius
    // приём, что и EntityConceal), но ТОЛЬКО ночью: фольклор называет защиту
    // конкретно сна и дурных снов, не восприятия вообще, а Морочники уже дают
    // отдельную ночную надбавку (NightPerceptionDistortionBonus,
    // GridWorldManagerEntities.cpp) — днём этому обережью буквально нечего
    // гасить прицельно, действие "и днём тоже" значило бы придумывать эффект
    // сверх того, что говорит фольклор.
    MorokReduction
};

// Инструмент сбора (DESIGN_Community_And_Homestead.md §2.3, множитель
// 2026-08-31, физический предмет + серебряный оберег 2026-09-06) —
// резак-ось таблицы множителей: голые руки безопасны для всего, но
// медленны; железо быстро, но вредит травам с флагом bIronAverse
// (карточки Плакун-травы/Чистотела); медь/кость — не-железные
// альтернативы, кость дополнительно бережёт травы с флагом bDelicate
// (карточка Медуницы). Владение (кроме голых рук — всегда доступны)
// резолвится по инвентарю через IngredientTableRow::bIsGatheringTool
// (AHerbalistPlayerController::SetGatheringTool). Оберег-ось (серебро,
// подавление проявлений при владении) — сознательно НЕ в этом enum,
// отдельный флаг bIsSilverWard, не резак вообще.
UENUM(BlueprintType)
enum class EGatheringTool : uint8
{
    BareHands,
    IronBlade,
    CopperBlade,
    BoneKnife
};

// Намерение сбора (DESIGN_Community_And_Homestead.md §2.4, PlantSeed,
// 2026-09-04) — прямой запрос: сбор ДЛЯ ПОСАДКИ в грядку сада должен
// давать другой результат, чем сбор ТОГО ЖЕ дикого растения на варку.
// Тот же самый Harvest() у уже существующего актора выдаёт либо обычный
// ингредиент (Brew, поведение до этой правки), либо посадочный материал
// (Seed, FInventoryItem::bIsPlantingStock) — не новый предмет в руке и не
// новый актор, тот же приём переключателя, что уже CurrentGatheringTool
// (SetGatheringTool): Exec-команда (SetHarvestIntent) меняет enum на
// контроллере, читается AGridWorldManager::OnResourceCollected так же, как
// CurrentGatheringTool.
UENUM(BlueprintType)
enum class EHarvestIntent : uint8
{
    Brew,
    Seed
};

UENUM(BlueprintType)
enum class EAlchemyOutcome : uint8
{
    Valid,
    BoiledWater,
    Ash,
    Catastrophe,
    // Ветка Bifurcation "Purification" (05_Systems.md) — раньше делила
    // значение Valid с обычной удачной варкой, из-за чего ничто ниже по
    // цепочке (UI, RecordFootprint) не могло отличить редкое драматичное
    // очищение от рядового успеха, вопреки прямому требованию ГДД.
    Purified
};

// ========== Базовые структуры ==========
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FDirection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Body = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Mind = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Spirit = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Nature = 0.f;

    void NormalizeSum()
    {
        Body = FMath::Max(0.0f, Body);
        Mind = FMath::Max(0.0f, Mind);
        Spirit = FMath::Max(0.0f, Spirit);
        Nature = FMath::Max(0.0f, Nature);
        float Sum = Body + Mind + Spirit + Nature;
        if (Sum > KINDA_SMALL_NUMBER)
        {
            Body /= Sum;
            Mind /= Sum;
            Spirit /= Sum;
            Nature /= Sum;
        }
        else
        {
            Body = Mind = Spirit = Nature = 0.25f;
        }
    }
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FMeta
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Distortion = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stability = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Purity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Potency = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Resonance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Corruption = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FRealState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDirection Direction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMeta Meta;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FEnvironment
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Toxicity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Fertility = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Moisture = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FMemoryState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float AccumulatedDistortion = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float StabilityMemory = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float HistoryPurity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float DistortionVelocity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float TimeOfLastDistortionChange = 0.0f;

    // Бистабильная релаксация (обсуждение в сессии 2026-08-24) — состояние
    // гистерезиса "клетка деградирует" (Corruption прошёл порог входа, цель
    // релаксации сама сдвинута к испорченному полюсу, пассивное восстановление
    // не работает). См. RegenerateCellParameters (GridWorldManagerCore.cpp).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    bool bDegrading = false;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FIntent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Coherence = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FRngState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 12345;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FWorldState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FEnvironment Env;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMemoryState Memory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntent Intent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState CurrentState;
};

// Доля присутствия биома в клетке (PCG-сплайн-регионы, 2026-08-31) —
// клетка на стыке нескольких ABiomeRegionVolume делит вес поровну между
// ними (1/N, не авторская "сила" региона — вертикальный срез). Только
// для взвешенного спавна ингредиентов (IngredientRegistrySubsystem::
// GetRandomResourceForBiome) — все остальные потребители биома (капища,
// хозяева места, биом-граф, тип воды) по-прежнему читают один дискретный
// FGridCell::Biome ниже, не эту структуру, см. комментарий у BiomeWeights.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FBiomeWeightEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBiomeType Biome = EBiomeType::MixedForest;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Weight = 1.0f;
};

// ========== Структура клетки ==========
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FGridCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0, Y = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBiomeType Biome = EBiomeType::MixedForest;

    // Разбивка биома на доли (PCG-сплайн-регионы, 2026-08-31,
    // AGridWorldManager::InitializeCells) — пусто = клетка вне всех
    // регионов, GetRandomResourceForBiome в этом случае откатывается на
    // {Biome: 1.0} выше, не крашит и не меняет поведение (обратная
    // совместимость с тестами, которые выставляют только Biome). Biome
    // выше — не производная от этого массива в общем случае, а
    // независимо посчитанная доминанта (см. InitializeCells) — оба поля
    // считаются одним проходом, не одно из другого.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBiomeWeightEntry> BiomeWeights;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState State;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState TargetState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FEnvironment Environment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMemoryState Memory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HarvestStress = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsWater = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WaterTypeID = NAME_None;

    // Проявленная сущность (см. 02_GDD/16_Entity_Manifestation.md) — чисто
    // презентационное поле (как HarvestStress/Memory), не часть Command/Delta
    // цикла: выставляется/снимается в AGridWorldManager::UpdateEntityManifestations,
    // тем же "внепайплайновым" каналом, что уже используют RegenerateCellParameters
    // и ApplyBiomeInfluences. Пусто = ничего не проявлено.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    FName ManifestedEntityID = NAME_None;

    // Физический актор, отражающий ManifestedEntityID выше (2026-08-30,
    // "заводим родительские классы для сущностей") — тот же слабый указатель,
    // что и ResourceActors ниже, синхронизируется в
    // AGridWorldManager::SyncManifestedEntityActor каждый раз, когда
    // ManifestedEntityID меняется в UpdateEntityManifestations.
    UPROPERTY()
    TWeakObjectPtr<class AHerbalistEntityActor> ManifestedEntityActor;

    // Список акторов ресурсов в этой клетке (слабые указатели, чтобы не мешать сборщику)
    UPROPERTY()
    TArray<TWeakObjectPtr<class AHerbalistResourceActor>> ResourceActors;

    // Что растёт в клетке, пока её чанк неактивен (2026-09-03, стриминг
    // сетки). Актор при деактивации уничтожается, а его IngredientID
    // переезжает сюда — при возврате игрока вырастает ровно то же, что
    // стояло, а не новый бросок кубика. Пусто у активной клетки: там
    // источник истины — сами акторы в ResourceActors выше.
    UPROPERTY()
    TArray<FName> DormantResourceIDs;

    // Прошла ли клетка первичное заселение. Отличает "пусто, потому что
    // собрано" от "пусто, потому что игрок сюда ещё не заходил": со
    // стримингом первичный бросок происходит не в InitializeCells, а при
    // первой активации чанка.
    UPROPERTY()
    bool bResourcesSeeded = false;

    // Сколько отдельных ресурсов сейчас отрастает в этой клетке (2026-09-04,
    // "а можно отрастание сделать поресурсно, а не по клеткам?"). Каждый
    // собранный ресурс запускает СВОЙ таймер (AGridWorldManager::
    // StartRegeneration) независимо от соседей -- клетка с 3 ресурсами,
    // из которой собрали один, не ждёт, пока опустеет целиком, чтобы
    // отрастить всё разом. Счётчик растёт на входе в StartRegeneration и
    // падает, когда таймер срабатывает (успешно ли -- не важно, попытка
    // исчерпана в любом случае) -- чисто наблюдаемое состояние, ни на что
    // игровое не влияет само по себе.
    UPROPERTY()
    int32 PendingRegrowthCount = 0;

    // Перо Жар-птицы (16_Entity_Manifestation.md §16.4, эндгейм-трофей) —
    // постоянная метка "никогда не деградирует". Единственный из четырёх
    // эффектов перьев с постоянным, не временным/одноразовым действием.
    // Клетка исключается из бистабильной релаксации/заражения соседей
    // (RegenerateCellParameters, GridWorldManagerCore.cpp) и из всех трёх
    // рангов проявления сущностей (UpdateEntityManifestations,
    // GridWorldManagerEntities.cpp) — не только Низшего/Легендарного, как
    // Шапка/Алконост, полная неприкосновенность, а не только защита от
    // амбиентной угрозы. Apply-команды (варка, вылитая прямо на клетку)
    // не гейтятся этим флагом — вне заявленного в задаче объёма.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    bool bEternallyPure = false;

    // Посадка (PlantSeed, DESIGN_Community_And_Homestead.md §2.4, 2026-09-04)
    // — в отличие от полива/State→BaseState (RegisterGardenPlot/GardenPlots
    // выше, вероятностный жест "подталкивания" в сторону нужного вида среди
    // кандидатов ниши), это явно посаженный конкретный вид семенем/черенком.
    // NAME_None = не посажено (обычное поведение, вероятностный выбор среди
    // кандидатов ниши не меняется). AGridWorldManager::SpawnOneResourceInCell
    // читает поле напрямую и, если оно не пусто, полностью обходит
    // PickWeightedResource — "здесь посажено именно это", не "склоняется к
    // этому". Персистентное поле (не одноразовый эффект): переживает
    // отрастание после сбора (StartRegeneration зовёт тот же
    // SpawnOneResourceInCell на той же Cell, поле не сбрасывается ни там, ни
    // при первичном заселении) — посадка держится, пока игрок не пересадит
    // другой вид тем же PlantSeed. Сохраняется в FSavedCellState
    // (HerbalistSaveTypes.h), тот же класс поля, что State/HarvestStress выше.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Garden")
    FName PlantedSpeciesID = NAME_None;
};

// ========== Сущности-"хозяева" (Основной уровень, 16_Entity_Manifestation §16.3) ==========
// Привязанный к конкретной клетке аккумулятор благосклонности — тот же принцип,
// что Restoration у капищ в 15_Cycles_And_Shrines, но без полноценной системы
// капищ: минимальная версия для одной клетки-"обиталища".
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FEntityLandmark
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    FName EntityID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    FIntPoint Cell = FIntPoint(-1, -1);

    // [-1, 1]: <0 — осквернено/разгневано, >0 — благосклонно. Растёт от бережного
    // сбора (высокая Purity, низкий HarvestStress клетки), падает от истощения.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    float Respect = 0.0f;
};

// ========== Базы/лагеря игрока (21_Journey_And_Artifacts.md §21.2, 2026-09-01) ==========
// Место за пределами Домового очага, где герой обжился — второй (и далее)
// дом, привязка для Клубочка (перемещение, шаг 5) и валидное место варки
// (IsValidBrewingLocation, GridWorldManagerBases.cpp).
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FHerbalistBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Bases")
    FIntPoint Cell = FIntPoint(-1, -1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Bases")
    EBiomeType Biome = EBiomeType::ForestSteppe;

    // Название базы — контент/локализация, не хардкодится здесь. NAME_None
    // до того, как левел-дизайнер/UI назначит реальное имя.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Bases")
    FName DisplayNameID = NAME_None;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FConditionModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDirection DeltaDirection;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaMagnitude = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaDistortion = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaStability = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaPurity = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaPotency = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaResonance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaCorruption = 0.0f;

    FConditionModifier()
    {
        DeltaDirection.Body = 0.0f;
        DeltaDirection.Mind = 0.0f;
        DeltaDirection.Spirit = 0.0f;
        DeltaDirection.Nature = 0.0f;
    }
};

struct PROJECTHERBALIST_API FAlatyr
{
    static const FRealState S0;
};

// ========== FInventoryItem ==========
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FInventoryItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName IngredientID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRealState State;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count = 1;

    // Время создания (мировое время, используется для расчёта порчи)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CreationTime = 0.0f;

    // Подвержен ли предмет порче (по умолчанию true для собранных ингредиентов)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSubjectToDecay = true;

    // Является ли водой — нужно Pipeline'у для правил варки (05_Systems.md:
    // обязательность воды, разбавление, штраф >80% воды), не требует обращения
    // к реестрам (UIngredientRegistrySubsystem/UWaterTypeRegistrySubsystem),
    // проставляется один раз при харвесте.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsWater = false;

    // Исход варки (ComputeApplyResult, PipelineV2.cpp) — раньше терялся сразу
    // после крафта: Valid/Purified/Catastrophe все сливались в один и тот же
    // IngredientID="Potion", различить их постфактум можно было только гадая
    // по числам State. Добавлено 2026-08-30 для фольклорной системы имён
    // (HerbalistNameUtils.cpp) — имя зелья должно честно отражать, ЧТО
    // случилось при варке (редкая удачная чистка, катастрофа), не только
    // пересчитанные задним числом оси. Не искажается восприятием намеренно,
    // в отличие от State/PerceivedState (JournalTypes.h): это категориальный
    // факт события, которое игрок только что физически наблюдал у котла
    // (сварилось — либо взорвалось), не скрытая числовая истина S_real,
    // добывать которую как раз и не должен Травник.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAlchemyOutcome BrewOutcome = EAlchemyOutcome::Valid;

    // Посадочный материал (DESIGN_Community_And_Homestead.md §2.4, PlantSeed,
    // 2026-09-04) — тот же IngredientID/ряд DT_IngredientClass, что и обычный
    // собранный ингредиент того же вида (вариант А из двух предложенных:
    // отдельное bool-поле проще нового FName-идентификатора без строки
    // таблицы), просто другое назначение предмета: собран с намерением
    // SetHarvestIntent("seed"), см. PipelineV2.cpp::ProcessHarvestCommand.
    // Годится только для AHerbalistPlayerController::PlantSeed, не для варки
    // напрямую (варка не проверяет этот флаг — тот же кусок растения годится
    // и в котёл, если игрок так решит, это не запрет, а лишь то, что PlantSeed
    // ищет именно такой предмет). УЧАСТВУЕТ в проверке стекуемости
    // (UHerbalistInventoryComponent::AreItemsStackable) — иначе посадочный
    // материал молча слился бы в один стек с обычным ингредиентом того же
    // вида (тот же IngredientID) при первом же AddItem, и оба назначения
    // потеряли бы смысл.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsPlantingStock = false;

    // Биом сбора (DESIGN_Community_And_Homestead.md §2.4, межбиомная варка,
    // 2026-09-04) — прямой запрос: поощрить варку из ингредиентов РАЗНЫХ
    // биомов (см. FApplyCommand::DistinctIngredientBiomeCount, ProcessApplyCommand,
    // PipelineV2.cpp). Проставляется один раз при харвесте, тем же принципом,
    // что уже bIsWater выше (не требует обращения к реестрам постфактум) —
    // PipelineV2::ProcessHarvestCommand уже держит в руках FGridCell с полем
    // Biome в момент сбора, отдельно протаскивать его через FHarvestCommand
    // не нужно. Дефолт MixedForest (а не первый по счёту Tundra) — нейтральный
    // "срединный" биом для предметов без честного источника (сваренные зелья,
    // старые сейвы без этого поля): тегированная сериализация USaveGame
    // (UHerbalistSaveGame::InventoryItems) заполняет отсутствующее поле именно
    // этим значением по умолчанию, не нулём enum'а.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBiomeType SourceBiome = EBiomeType::MixedForest;

    // Сушка (DESIGN_Community_And_Homestead.md §2.2, "Хранилища" пункт 3,
    // 2026-09-04, прямой запрос пользователя: "сушка — не контейнер, а
    // ПРОЦЕСС... растянутый во игровое время, не мгновенно") — в отличие от
    // EStorageContainerType (множитель decay ТАРЫ, применяется мгновенно к
    // уже собранному предмету), сушка ПРЕВРАЩАЕТ сам предмет: свежая трава
    // портится независимо от того, в чём её несут, сушёная — на порядки
    // медленнее (см. DriedItemDecayMultiplier, HerbalistSettings.h), и у
    // части растений честно меняет алхимические оси (FIngredientTableRow::
    // DriedStateDelta) — не только сохранность, реальная травническая
    // практика (эфирные масла улетучиваются, действующие вещества
    // концентрируются при удалении воды, у части ядовитых/галлюциногенных
    // растений сам характер действия меняется, см. компендиумные карточки
    // с разделом "## Сушка").
    //
    // false (дефолт) = свежий предмет, обычное поведение. Ставится true
    // ровно один раз, когда DryingTimeRemainingSeconds досчитывает до нуля
    // (UHerbalistInventoryComponent::TickDryingItem) — терминальный переход
    // вперёд, как у bIsPlantingStock/BrewOutcome выше, не откатывается назад.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsDried = false;

    // Таймер процесса сушки, В СЕКУНДАХ РЕАЛЬНОГО ВРЕМЕНИ — тот же принцип,
    // что уже DecayUpdateInterval/ApplyDecayToItem в этом же компоненте
    // (реальные секунды тика, не GameClockSeconds моста, которого у
    // UHerbalistInventoryComponent нет и не должно быть — см. довод у
    // bIsDryingRack, HerbalistInventoryComponent.h).
    //
    // -1 (сентинел) = предмет не сушится и никогда не сушился. При первом
    // тике внутри инвентаря-сушилки (bIsDryingRack=true) взводится в
    // DryingDurationSeconds (HerbalistSettings.h) и с каждым тиком считает
    // вниз; по достижении <=0 предмет становится bIsDried=true выше, поле
    // фиксируется на 0. ВАЖНОЕ СВОЙСТВО, не отдельный код: таймер тикает
    // ТОЛЬКО пока предмет физически лежит в инвентаре с bIsDryingRack=true
    // (TickComponent проверяет свой собственный флаг) — переложенный в
    // обычный карман/корзину предмет просто перестаёт досчитывать, ровно
    // как в жизни: снял с сушила — не досохнет сам по себе, повесил
    // обратно — досчитает с того же места.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DryingTimeRemainingSeconds = -1.0f;

    // Отстой (2026-09-05, многоступенчатые зелья -- прямой запрос
    // пользователя: "2. Отстой — процесс усиления доминирующей оси") --
    // ТОТ ЖЕ сентинел-паттерн, что уже DryingTimeRemainingSeconds/bIsDried
    // выше: -1 = не начато, взводится при первом тике внутри инвентаря
    // ASettlingStandActor (EProcessingStationType::SettlingStand,
    // HerbalistInventoryComponent.h) и считает вниз ТОЛЬКО пока предмет
    // физически там лежит. Отдельная, независимая пара полей от Drying --
    // проект уже держит параллельные наборы полей на похожие, но разные
    // механики (три независимых набора у оберегов, GridWorldManagerWards.cpp),
    // не один "универсальный процесс": Отстой усиливает доминирующую ось
    // Direction ценой Magnitude, Сушка меняет Meta ценой ничего -- разная
    // формула эффекта у каждого, обобщать таймер без обобщения эффекта
    // означало бы делить код там, где он всё равно снова разойдётся.
    //
    // bHasSettled -- терминальный флаг, тем же принципом, что bIsDried:
    // эффект применяется РОВНО ОДИН РАЗ (TickComponent проверяет этот флаг
    // перед вызовом TickSettlingItem), повторный тик уже отстоявшегося
    // зелья не удваивает усиление.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasSettled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SettlingTimeRemainingSeconds = -1.0f;

    // Выпаривание (2026-09-05, тот же запрос -- "5. Выпаривание") -- тот же
    // сентинел-паттерн, ещё одна независимая пара полей (см. довод у
    // bHasSettled выше): концентрирует Magnitude/Potency ценой роста
    // Distortion/Corruption (см. UHerbalistInventoryComponent::
    // ApplyEvaporationEffect) -- третья, отдельная формула эффекта, третья
    // отдельная пара полей, тот же принцип.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasEvaporated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EvaporationTimeRemainingSeconds = -1.0f;

    bool IsEmpty() const { return IngredientID.IsNone() || Count <= 0; }
    void Clear() { IngredientID = NAME_None; State = FRealState(); Count = 0; CreationTime = 0.0f; bSubjectToDecay = true; }
    bool IsValid() const { return !IngredientID.IsNone() && Count > 0; }
};

// ========== L2 Vector Direction ==========
// ... (без изменений)
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FL2Direction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Body = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Mind = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Spirit = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Nature = 0.0f;

    void NormalizeL2(FRngState& Rng)
    {
        float LenSq = Body * Body + Mind * Mind + Spirit * Spirit + Nature * Nature;
        if (LenSq > KINDA_SMALL_NUMBER)
        {
            float InvLen = FMath::InvSqrt(LenSq);
            Body *= InvLen;
            Mind *= InvLen;
            Spirit *= InvLen;
            Nature *= InvLen;
        }
        else
        {
            auto Rand01 = [&Rng]() {
                Rng.Seed = (Rng.Seed * 196314165) + 907633515;
                return (Rng.Seed & 0x00FFFFFF) / float(0x01000000);
                };
            float x1, x2, x3, x4, s;
            do {
                x1 = Rand01() * 2.0f - 1.0f;
                x2 = Rand01() * 2.0f - 1.0f;
                x3 = Rand01() * 2.0f - 1.0f;
                x4 = Rand01() * 2.0f - 1.0f;
                s = x1 * x1 + x2 * x2 + x3 * x3 + x4 * x4;
            } while (s > 1.0f || s < KINDA_SMALL_NUMBER);
            float InvLen = FMath::InvSqrt(s);
            Body = x1 * InvLen;
            Mind = x2 * InvLen;
            Spirit = x3 * InvLen;
            Nature = x4 * InvLen;
        }
    }

    FDirection ToL1() const
    {
        FDirection Result;
        Result.Body = FMath::Max(0.0f, Body);
        Result.Mind = FMath::Max(0.0f, Mind);
        Result.Spirit = FMath::Max(0.0f, Spirit);
        Result.Nature = FMath::Max(0.0f, Nature);
        float Sum = Result.Body + Result.Mind + Result.Spirit + Result.Nature;
        if (Sum > KINDA_SMALL_NUMBER)
        {
            Result.Body /= Sum;
            Result.Mind /= Sum;
            Result.Spirit /= Sum;
            Result.Nature /= Sum;
        }
        else
        {
            Result.Body = Result.Mind = Result.Spirit = Result.Nature = 0.25f;
        }
        return Result;
    }

    float Length() const
    {
        return FMath::Sqrt(Body * Body + Mind * Mind + Spirit * Spirit + Nature * Nature);
    }
};

inline FL2Direction ToL2(const FDirection& L1, FRngState& Rng)
{
    FL2Direction Result;
    Result.Body = L1.Body;
    Result.Mind = L1.Mind;
    Result.Spirit = L1.Spirit;
    Result.Nature = L1.Nature;
    Result.NormalizeL2(Rng);
    return Result;
}