// HerbalistSaveTypes.h
//
// Сохранения v1 (ROADMAP.md §1, "Что я бы сделал первым"). Плотное — все
// клетки, не разреженные отклонения: при текущем размере сетки (20x20, ~400
// клеток) это не тяжелее пары десятков КБ (DESIGN_World_State.md §2), а
// разреженная модель — оптимизация для сетки на порядки крупнее (Шаги 2-5
// миграции хранения, ещё не сделаны). Biome/вода/начальный ростер ресурсов
// не сохраняются — они уже детерминированная функция (RngBaseSeed, порядок
// InitializeCells), сохраняется только то, что отличается от неё после игры:
// State/TargetState/HarvestStress/Memory/ManifestedEntityID и то, какие
// именно ресурсы сейчас заспавнены в клетке (а не пересобраны броском кубика).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Journal/JournalTypes.h"
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
    // будущая погода (UltraDynamicSky, ROADMAP.md Фаза D §12) должны
    // возобновляться с той же точки, а не с рассвета каждой новой сессии.
    UPROPERTY()
    float GameClockSeconds = 0.0f;

    UPROPERTY()
    TArray<FSavedCellState> Cells;

    UPROPERTY()
    TArray<FEntityLandmark> EntityLandmarks;

    UPROPERTY()
    TArray<FShrine> Shrines;

    UPROPERTY()
    TArray<FInventoryItem> InventoryItems;

    UPROPERTY()
    TArray<FJournalEntry> JournalEntries;

    UPROPERTY()
    FVector PlayerLocation = FVector::ZeroVector;

    UPROPERTY()
    FRotator PlayerRotation = FRotator::ZeroRotator;
};
