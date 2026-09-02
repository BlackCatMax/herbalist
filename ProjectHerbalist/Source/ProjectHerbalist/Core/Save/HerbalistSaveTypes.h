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
};

UCLASS()
class PROJECTHERBALIST_API UHerbalistSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
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

    UPROPERTY()
    TArray<FJournalEntry> JournalEntries;

    UPROPERTY()
    FVector PlayerLocation = FVector::ZeroVector;

    UPROPERTY()
    FRotator PlayerRotation = FRotator::ZeroRotator;
};
