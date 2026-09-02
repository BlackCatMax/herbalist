// Core/Simulation/Public/CommandTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"      // FRealState, FIntent, FRngState
#include "Core/Types/BiomeTypes.h"              // EBiomeType

enum class ECommandPrimitive : uint8
{
    None,
    Query,
    Transfer,
    Apply,
    Harvest,
    Wait,
    // Реанимирован 2026-08-31 (DESIGN_Community_And_Homestead.md §1.1) —
    // удалён 2026-08-30 как мёртвый код (ни один путь не создавал
    // FTalkCommand), но запрошены настоящие диалоговые деревья тем же днём
    // позже — сознательный откат, не забытая чистка. Как и Query, не
    // обрабатывается в ExecutePipeline (см. default-ветку там) — диалог
    // читает Molva/Respect и статический реестр (Core/Dialogue/
    // DialogueTypes.h), ничего не меняет в FRealState/инвентаре, поэтому не
    // производит Delta и не нуждается в детерминированном пайплайне; сама
    // команда реанимирована ради полноты алгебры Q/T/D/S/B, не как
    // фактический канал эффекта.
    Talk
};

// Q – запрос (какие клетки/предметы затронуты)
struct FQueryCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Query;
    TArray<FIntPoint> TargetCells;
    TArray<int32> TargetInventorySlots;
};

// T – перемещение (Transfer)
struct FTransferCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Transfer;
    int32 SourceContainerID = 0;
    int32 TargetContainerID = 0;
    FName IngredientID;
    int32 Amount = 1;
};

// D – применить зелье/предмет к клетке (Apply), либо создать зелье (крафт)
struct FApplyCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Apply;
    FIntPoint TargetCell;
    TArray<FInventoryItem> Ingredients;
    FIntent Intent;
    bool bIsCrafting = false;   // true = крафт в инвентарь, false = применение на клетку

    // Итог правильно исполненного ритуала (AGridWorldManager::TryAdvanceRitual,
    // Core/Alchemy/RitualTypes.h, 2026-08-30) -- градации опасности по числу
    // ингредиентов (ComputeApplyResult) для него не действуют: игрок сварил
    // по верному порядку/месту/времени, не закинул всё разом, котёл не
    // наказывает за укрощённую, а не проигнорированную сложность.
    bool bIsRitual = false;

    // Камень-оберег (21_Journey_And_Artifacts.md §21.3, 2026-09-01) --
    // резолвится вне Pipeline, тем же принципом, что bIsRitual выше:
    // вызывающая сторона (AGridWorldManager::ApplyAlchemyResult/
    // HasUnspentBifurcationCharm, AlchemyTransferWidget.cpp) проверяет
    // AcquiredArtifacts перед постановкой команды в очередь, Pipeline не
    // лезет в мировое состояние сам. НЕ прошито для завершения ритуала
    // (GridWorldManagerRitual.cpp) -- тот путь идёт мимо обычной очереди
    // команд/RunSimulationStep, куда привязан пост-обработка списания
    // заряда, и ритуалы и так уже обходят градации риска Bifurcation
    // (RiskyCount=0 при bIsRitual) -- отдельная, более редкая задача.
    bool bBifurcationCharmActive = false;

    // Лунный цикл (15_Cycles_And_Shrines.md §15.3, 2026-09-02, Tier 1 п.1.2) --
    // "вместе с силой растёт и Morok... ставки выше в обе стороны", часть
    // текста строки Полнолуния, до сих пор реализованная только для сбора
    // (FHarvestCommand::MoonPhase, GenerateHarvestResult). Тот же принцип
    // "резолвится вне Pipeline" (см. bBifurcationCharmActive/bIsRitual выше):
    // вызывающая сторона (AGridWorldManager::ApplyAlchemyResult) читает
    // GetMoonPhase(). НЕ прошито для завершения ритуала
    // (GridWorldManagerRitual.cpp) -- тот путь идёт мимо обычной очереди
    // команд, тот же известный, отдельно задокументированный разрыв, что и у
    // bBifurcationCharmActive.
    EMoonPhase MoonPhase = EMoonPhase::NewMoon;
};

// S – сбор ресурса (Harvest)
struct FHarvestCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Harvest;
    FIntPoint TargetCell;
    FName IngredientID;
    int32 Amount = 1;

    // Базовые параметры ингредиента (IngredientTableRow::BaseState), резолвятся
    // вне Pipeline (у AHerbalistResourceActor уже есть на момент сбора — см.
    // AGridWorldManager::OnResourceCollected), чтобы Pipeline не обращался к реестрам.
    FRealState BaseState;

    // IngredientTableRow::Resilience — насколько трава сопротивляется характеру
    // места при сборе (0 = целиком принимает биом, 1 = остаётся собой).
    float Resilience = 0.f;

    // AGridWorldManager::GetMoonPhase(), резолвится вне Pipeline тем же
    // принципом, что BaseState/Resilience выше (15_Cycles_And_Shrines.md
    // §15.3: Растущая усиливает Body/Nature/Magnitude, Полнолуние —
    // Spirit/Potency/Resonance; см. GenerateHarvestResult в PipelineV2.cpp).
    EMoonPhase MoonPhase = EMoonPhase::NewMoon;

    // Инструмент сбора (DESIGN_Community_And_Homestead.md §2.3, 2026-08-31)
    // и копии IngredientTableRow::bIronAverse/bDelicate — тот же принцип,
    // что BaseState/Resilience выше: резолвятся вне Pipeline
    // (AGridWorldManager::OnResourceCollected из AHerbalistResourceActor и
    // AHerbalistPlayerController::CurrentGatheringTool), не лезут в реестр
    // изнутри симуляции. См. ToolQualityMultiplier в PipelineV2.cpp.
    EGatheringTool Tool = EGatheringTool::BareHands;
    bool bIronAverse = false;
    bool bDelicate = false;
};

// B – диалог (реанимирован 2026-08-31, см. комментарий у ECommandPrimitive
// ::Talk выше — был убран 2026-08-30 как мёртвый код, сознательно
// восстановлен тем же днём позже по прямому запросу настоящих диалоговых
// деревьев). DialogueID — ключ в Core/Dialogue::GetDialogueDefinitions(),
// не сама диалоговая структура: тело дерева и текущий узел разговора не
// часть команды/Delta, живут на стороне PlayerController (см.
// CurrentDialogueNodeID) — команда лишь фиксирует факт "поговорил",
// не переносит состояние разговора через границу пайплайна.
struct FTalkCommand
{
    FName DialogueID;
};

// Контейнер команды (одна запись в пакете команд).
struct FCommandEntry
{
    ECommandPrimitive Primitive = ECommandPrimitive::None;

    FQueryCommand       Query;
    FTransferCommand    Transfer;
    FApplyCommand       Apply;
    FHarvestCommand     Harvest;
    FTalkCommand        Talk;

    uint32 CommandID = 0;
    bool bCancelled = false;
};

// Пакет команд одного тика — переименован из FCommandGraph 2026-08-30
// ("закрываем архитектурный долг", DESIGN_World_State.md §23): имя обещало
// граф зависимостей между командами, а внутри всегда был плоский
// TArray<FCommandEntry>, исполняемый по порядку добавления
// (PipelineV2.cpp::ExecutePipeline, обычный range-for по Commands). Заодно
// убран мёртвый ExecutionOrder (объявлен, нигде не читался и не
// сортировался по нему — реальный порядок исполнения всегда был порядком
// добавления, не этим полем). Честное имя вместо попытки построить
// настоящий граф зависимостей, которого никакая текущая команда не просит:
// все шесть примитивов (Query/Transfer/Apply/Harvest/Wait) независимы друг
// от друга внутри одного тика.
struct FCommandBatch
{
    TArray<FCommandEntry> Commands;

    int32 TickID = 0;
    double Timestamp = 0.0;
    bool bIsValid = true;

    void Clear()
    {
        Commands.Empty();
        bIsValid = true;
    }

    void AddCommand(const FCommandEntry& Cmd)
    {
        Commands.Add(Cmd);
    }

    int32 Num() const { return Commands.Num(); }
};