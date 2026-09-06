// HerbalistSaveTypes.h
//
// Сохранения v1 (CHANGELOG.md 2026-08-24, "Что я бы сделал первым"). Плотное — все
// клетки, не разреженные отклонения: при текущем размере сетки (20x20, ~400
// клеток) это не тяжелее пары десятков КБ (DESIGN_World_State.md §2), а
// разреженная модель — оптимизация для сетки на порядки крупнее (Шаги 2-5
// миграции хранения, ещё не сделаны). Biome/вода/начальный ростер ресурсов
// не сохраняются — они уже детерминированная функция (RngBaseSeed, порядок
// InitializeCells; с 2026-08-31 биом ещё и функция расставленных в уровне
// ABiomeRegionVolume — по-прежнему НЕ сохраняется отдельно: это контент
// уровня, стабильный между сессиями сам по себе, не рантайм-состояние),
// сохраняется только то, что отличается от неё после игры:
// State/TargetState/HarvestStress/Memory/ManifestedEntityID и то, какие
// именно ресурсы сейчас заспавнены в клетке (а не пересобраны броском кубика).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Journal/JournalTypes.h"
#include "Core/Entities/ArtifactTypes.h"
#include "Core/Shrine/ShrineTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/BiomeGraph/BiomeGraphTypes.h"
#include "HerbalistSaveTypes.generated.h"

USTRUCT()
struct PROJECTHERBALIST_API FSavedCellState
{
    GENERATED_BODY()

    UPROPERTY()
    int32 X = 0;

    UPROPERTY()
    int32 Y = 0;

    UPROPERTY()
    FRealState State;

    UPROPERTY()
    FRealState TargetState;

    UPROPERTY()
    float HarvestStress = 0.0f;

    UPROPERTY()
    FMemoryState Memory;

    UPROPERTY()
    FName ManifestedEntityID = NAME_None;

    // Что реально заспавнено в клетке прямо сейчас — не переигрывается через
    // WorldRNG (собранное игроком не должно тихо вернуться после загрузки).
    // Пусто = ничего (собрано и ждёт восстановления, либо клетка без ресурсов).
    UPROPERTY()
    TArray<FName> ResourceIngredientIDs;

    // Перо Жар-птицы (16_Entity_Manifestation.md §16.4, 2026-09-02) —
    // постоянная метка, обязана пережить сохранение, тот же класс поля,
    // что ManifestedEntityID выше.
    UPROPERTY()
    bool bEternallyPure = false;

    // Посадка (PlantSeed, DESIGN_Community_And_Homestead.md §2.4, 2026-09-04)
    // — игровое решение игрока (PlantSeedInCell), не производная от клетки,
    // тот же класс поля, что ManifestedEntityID/bEternallyPure выше: без
    // этого перезагрузка молча стирала бы все посадки, откатывая грядку
    // обратно к вероятностному GetRandomResourceForNiche.
    UPROPERTY()
    FName PlantedSpeciesID = NAME_None;

    // Аудит 2026-09-05: "дальние клетки повторно засеиваются в новой сессии,
    // хотя были собраны" — GridWorldManagerCore.cpp::UpdateStreamingChunks
    // решает, сеять ли ресурсы заново (SpawnResourcesInCell, случайный
    // бросок) или поднимать сохранённый DormantResourceIDs-ростер, ИМЕННО
    // по этому флагу. Без него он всегда false в новой сессии, и клетка,
    // уже собранная/пересеянная игроком, при первой же активации чанка
    // получает свежий случайный бросок вместо восстановленного из сейва
    // (который ApplySaveCells к этому моменту уже честно применил).
    UPROPERTY()
    bool bResourcesSeeded = false;
};

// Домашние хранилища (DESIGN_Community_And_Homestead.md §2.2, 2026-09-04) —
// аудит 2026-09-05: "и их содержимое не сохраняются вообще". AStorageContainer
// нигде не отслеживается постоянным списком (ни на AGridWorldManager, ни на
// контроллере) — единственный источник истины сейчас, как и у самого
// BuildHomeStorage при проверке дубликатов, TActorIterator по миру. Позиция
// не сохраняется отдельно: контейнер всегда пересоздаётся у ТЕКУЩЕЙ
// клетки-якоря дома (AAlchemyTableActor), тем же путём, что и исходная
// постройка (SpawnHomeStorageContainer) — сохранять её независимо незачем,
// как и Shrine.Cell/позицию AAlchemyTableActor (контент уровня, не
// рантайм-состояние).
USTRUCT()
struct PROJECTHERBALIST_API FSavedHomeStorage
{
    GENERATED_BODY()

    UPROPERTY()
    EStorageContainerType ContainerType = EStorageContainerType::None;

    UPROPERTY()
    TArray<FInventoryItem> Items;
};

// Тиражные обереги (награда ритуалов перехода ярусов биомов, 2026-09-04,
// GridWorldManagerWards.cpp::ActivateTieredWard) — аудит 2026-09-05,
// решение пользователя (а): постоянная награда за завершённый ритуал, БЕЗ
// таймера ("как активировал, так и работает") — в отличие от шести
// GameClockSeconds-таймеров "короткого окна" (Ward*/InvisibilityCap/
// YouthApple/Alkonost, см. ResetSessionOnlyWardTimers), которые
// продолжают НЕ персистится намеренно, тиражные обереги — долгоживущий
// прогресс и обязаны пережить перезагрузку так же, как AcquiredArtifacts/
// AcquiredFeathers.
USTRUCT()
struct PROJECTHERBALIST_API FSavedTieredWards
{
    GENERATED_BODY()

    UPROPERTY()
    bool bConcealmentActive = false;

    UPROPERTY()
    TArray<EBiomeType> ConcealmentHomeBiomes;

    UPROPERTY()
    bool bMorokReductionActive = false;

    UPROPERTY()
    TArray<EBiomeType> MorokReductionHomeBiomes;

    UPROPERTY()
    bool bBrewBoostActive = false;

    UPROPERTY()
    TArray<EBiomeType> BrewBoostHomeBiomes;
};

UCLASS()
class PROJECTHERBALIST_API UHerbalistSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // Версия формата (аудит 2026-09-05: "версии сейва нет вовсе — некуда
    // будет добавить миграцию, когда понадобится"). Начинается с 1 —
    // текущий (v1) формат этого файла, а не "версия ещё не введена". Любое
    // будущее структурное изменение (переименование/удаление поля, смена
    // семантики) обязано инкрементировать это число и добавить ветку
    // миграции в UHerbalistSaveSubsystem::LoadGame, а не молча ломать старые
    // сохранения или полагаться на то, что новые UPROPERTY-поля тихо
    // получат дефолт при десериализации старого файла (для добавленных
    // полей это само по себе безопасно — именно поэтому все поля этого
    // класса ниже уже имеют дефолтные значения; версия нужна для СЛУЧАЕВ
    // ПОСЛОЖНЕЕ простого добавления поля).
    UPROPERTY()
    int32 SaveVersion = 1;

    UPROPERTY()
    int32 RngBaseSeed = 12345;

    UPROPERTY()
    int32 GridSizeX = 20;

    UPROPERTY()
    int32 GridSizeY = 20;

    UPROPERTY()
    int32 CurrentTickID = 0;

    // Игровые часы (AGridWorldManager::GameClockSeconds) — фаза суток и
    // будущая погода (UltraDynamicSky, ROADMAP.md "Реальный Ultra Dynamic
    // Weather") должны возобновляться с той же точки, а не с рассвета
    // каждой новой сессии.
    UPROPERTY()
    float GameClockSeconds = 0.0f;

    UPROPERTY()
    TArray<FSavedCellState> Cells;

    UPROPERTY()
    TArray<FEntityLandmark> EntityLandmarks;

    UPROPERTY()
    TArray<FShrine> Shrines;

    // Домашние хранилища (аудит 2026-09-05) — см. подробный довод у
    // FSavedHomeStorage выше.
    UPROPERTY()
    TArray<FSavedHomeStorage> HomeStorages;

    // Молва общины (DESIGN_Community_And_Homestead.md §1, 2026-08-31) — тот
    // же принцип, что Shrines/EntityLandmarks выше: растёт/падает только
    // явным подношением, без пассивного спада, значит обязана переживать
    // сохранение так же, как Restoration/Respect — иначе перезагрузка молча
    // обнуляла бы репутацию, противореча собственному правилу "у подношения
    // нет срока годности". Найдено и закрыто аудитом сразу после реализации,
    // не отдельным проходом.
    UPROPERTY()
    float Molva = 0.0f;

    // Сад (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31) — аудит "на
    // аудит" (2026-08-31) нашёл тот же класс пробела, что уже закрыт для
    // Molva выше, только из более раннего прохода (Сад): AGridWorldManager::
    // GardenPlots нигде не сохранялся, хотя SetGardenPlot -- игровое решение
    // игрока, не деривация из клетки. Без этого поля перезагрузка молча
    // стирала бы все назначенные пристройки (грибница/погреб/водоём/грядки).
    UPROPERTY()
    TMap<FIntPoint, EGardenNiche> GardenPlots;

    // Базы/лагеря (21_Journey_And_Artifacts.md §21.2, 2026-09-01) — тот же
    // класс поля, что GardenPlots выше: игровое решение игрока (RegisterBase
    // вызывается только через Exec-команду FoundBase), не деривация из
    // клетки. Тот же аудиторский урок — не забыть сохранить на этот раз.
    UPROPERTY()
    TArray<FHerbalistBase> Bases;

    // Артефакты Легендарных (§21.3-21.4, 2026-09-01) — тот же класс поля,
    // что Bases выше.
    UPROPERTY()
    TArray<FAcquiredArtifact> AcquiredArtifacts;

    // Тиражные обереги (аудит 2026-09-05, решение пользователя (а)) — см.
    // подробный довод у FSavedTieredWards выше: постоянная награда за
    // ритуал, тот же класс поля, что AcquiredArtifacts.
    UPROPERTY()
    FSavedTieredWards TieredWards;

    // Серебряный оберег (Ось Б §2.3, 2026-09-06) — экипированный, постоянный,
    // без таймера, тот же класс поля, что TieredWards выше (не путать с
    // шестью session-only Ward*-таймерами, которые НЕ сохраняются вовсе).
    UPROPERTY()
    bool bSilverWardActive = false;

    // Курганы (§2.3/§4.3, 2026-09-06) — игровое состояние (какие курганы уже
    // разграблены), не деривация из клетки, тот же класс поля, что
    // GardenPlots выше. Разграбленный курган отсутствует в карте (см.
    // AGridWorldManager::LootKurgan) — без сохранения перезагрузка молча
    // "воскрешала" бы уже найденные Костяной нож/Серебряный оберег.
    UPROPERTY()
    TMap<FIntPoint, FName> KurganSites;

    // Биомный граф — накопленные поля (AUDIT_AND_REFACTORING_PLAN.md §7.1,
    // 2026-09-06, решение пользователя: "граф должен переживать сохранение").
    // FBiomeGraphNode::MorokField/ZaryanaField помечены Transient (обычная
    // UPROPERTY-сериализация их не видит намеренно) — без этого поля граф
    // откатывался бы к статическим дефолтам DA_BiomeGraph при каждой
    // загрузке, хотя видимый в клетках эффект накопленного влияния (уже
    // просочившийся в Cell.TargetState через тактический фикс §7.1)
    // корректно переживает сохранение и без этого. Сохраняется узел
    // целиком (включая статические MorokAffinity/ZaryanaAffinity/Stability,
    // не только динамическую часть) — UBiomeGraphSubsystem::
    // RestoreNodeFieldState сам выбирает из этой карты только MorokField/
    // ZaryanaField/Memory, статику из уже загруженного DA_BiomeGraph не
    // трогает.
    UPROPERTY()
    TMap<FName, FBiomeGraphNode> BiomeGraphNodes;

    // Точки интереса, §4 (2026-09-06, см. POITypes.h) — сохранены как
    // конкретные координаты, не пересеиваются при загрузке: детерминированный
    // сев зависит от порядка вызовов WorldRNG внутри InitializeCells, а к
    // моменту загрузки WorldRNG уже другой (сдвинут прошедшей сессией) — тот
    // же довод, что уже у KurganSites выше про "молча воскрешать". Соловей
    // хранит ещё и bSoloveyTriggered — единственный из новых POI с
    // одноразовым игровым эффектом, требующим отдельного флага, не только
    // координаты.
    UPROPERTY()
    FIntPoint TotemSite = FIntPoint(-1, -1);

    UPROPERTY()
    FIntPoint SvetloyarSite = FIntPoint(-1, -1);

    UPROPERTY()
    FIntPoint GoryuchKamenSite = FIntPoint(-1, -1);

    UPROPERTY()
    FIntPoint SoloveySite = FIntPoint(-1, -1);

    UPROPERTY()
    bool bSoloveyTriggered = false;

    // Усмирение плакун-травой (§4.4, 2026-09-06) -- отдельно от
    // bSoloveyTriggered, см. довод у AGridWorldManager::CalmSolovey.
    UPROPERTY()
    bool bSoloveyCalmed = false;

    // Калинов мост / Трёхглавый Змей — координата сохраняется для сева
    // (юнит 2/2), сам Landmark (ЗмейГорыныч) уже сохраняется отдельно
    // через EntityLandmarks выше в этом файле, не задвоено здесь.
    UPROPERTY()
    FIntPoint KalinovMostSite = FIntPoint(-1, -1);

    // Заряна (обсуждение в сессии 2026-08-24) — Clarity/Буян/собранные ID
    // растут медленно и редко, ровно то, что должно переживать сохранение.
    UPROPERTY()
    float GlobalPerceptionClarity = 0.0f;

    // Якорь Clarity (20_Investment_And_Progression.md §20.3, 2026-09-01) —
    // тот же принцип, что GlobalPerceptionClarity выше: растёт редко,
    // должен пережить сохранение, восстанавливается как есть, без пересчёта.
    UPROPERTY()
    float ClarityAnchor = 0.0f;

    // Сглаженный отклик (§20.3, 2026-09-02) — переживает сохранение, тот же
    // довод, что уже ClarityAnchor выше: без этого перезагрузка сбрасывала
    // бы уже накопленную сходимость к нулю, мгновенно меняя видимую Clarity.
    UPROPERTY()
    float ClarityResponseSmoothed = 0.0f;

    // Перья вещих птиц (16_Entity_Manifestation.md §16.4, 2026-09-02) — тот
    // же класс поля, что AcquiredArtifacts выше.
    UPROPERTY()
    TArray<FName> AcquiredFeathers;

    // Перо Гамаюна съедено — перманентный флаг, переживает потерю самого
    // Пера (расходуется на поедание, AcquiredFeathers его больше не несёт).
    UPROPERTY()
    bool bGamayunPropheticGuaranteed = false;

    // Роса (19_Rosa_Signal.md §19.2, Слой 2) — разовая метка на партию,
    // персистится, чтобы "первое совпадение" не срабатывало заново после
    // каждой перезагрузки. ZaryanaCell саму не сохраняем — она левел-контент
    // (тот же принцип, что позиция AAlchemyTableActor/Shrine.Cell), не
    // рантайм-состояние.
    UPROPERTY()
    bool bRosaFirstFalseSignalShown = false;

    UPROPERTY()
    bool bBuyanReached = false;

    // Три исхода у Буяна (18_Ending.md §18.1, 2026-09-01) — тот же класс
    // поля, что bBuyanReached выше: не переигрывается, обязан пережить
    // перезагрузку.
    UPROPERTY()
    EBuyanPath ChosenBuyanPath = EBuyanPath::None;

    UPROPERTY()
    TArray<FName> CollectedFragmentIDs;

    // Предметы-спутники (21_Journey_And_Artifacts.md §21.2) — постоянный
    // дар, не должен теряться при перезагрузке (контроллерное состояние,
    // не мировое, но тот же принцип, что PlayerLocation ниже: сохраняем
    // напрямую из PC в SaveGame()/LoadGame()).
    UPROPERTY()
    bool bHasMirror = false;

    UPROPERTY()
    bool bHasYarnBall = false;

    UPROPERTY()
    TArray<FInventoryItem> InventoryItems;

    // Экипированный переносной контейнер (аудит 2026-09-05: "эффект
    // экипированного контейнера теряется между сессиями") — игровое решение
    // игрока (EquipContainer), не деривация из инвентаря, тот же класс поля,
    // что GardenPlots/Bases выше. BeginPlay контроллера сам выставляет
    // Basket по умолчанию до вызова LoadGame() (см. подробный комментарий
    // там) — этим полем LoadGame честно перекрывает тот дефолт, если игрок
    // успел сменить контейнер до сохранения.
    UPROPERTY()
    EStorageContainerType PersonalContainerType = EStorageContainerType::Basket;

    UPROPERTY()
    TArray<FJournalEntry> JournalEntries;

    UPROPERTY()
    FVector PlayerLocation = FVector::ZeroVector;

    UPROPERTY()
    FRotator PlayerRotation = FRotator::ZeroRotator;
};
