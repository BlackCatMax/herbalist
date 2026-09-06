// GatheringToolAppendCommandlet.h
//
// Добавляет ряды для инструментов сбора и оберега-при-сборе
// (DESIGN_Community_And_Homestead.md §2.3, "полировка" 2026-09-06) в живой
// DT_IngredientClass — тот же UDataTable::AddRow-приём, что уже
// ContainerAppendCommandlet/WardCrystalAppendCommandlet (НИКОГДА
// GetTableAsJSON()/CreateTableFromJSONString()).
//
// Четыре ряда: Железный серп/Медный серп/Костяной нож (Ось А — резак,
// IngredientTableRow::bIsGatheringTool + GatheringToolType) и Серебряный
// оберег (Ось Б — bIsSilverWard, отдельный флаг, не резак вообще). Все
// четыре -- утварь/артефакт, не растение/минерал сбора: AllowedBiomes пуст
// (не собираются в мире), GardenNiche::None (сад их не касается), DecayRate=0
// (не портятся), Resilience=1.0 (тот же приём, что уже у контейнеров/
// тиражных оберегов — сад никогда не тянет их к BaseState места, в отличие
// от кристаллов Пещеры, которые Resilience=0 сознательно). Class=Catalyst
// -- тот же выбор, что у контейнеров (утварь, не сырьё варки).
//
// Источник каждого — прямое продолжение таблицы §2.3: Железный серп выдаётся
// стартовым инвентарём (AHerbalistPlayerController::BeginPlay, не эта
// таблица), Медный серп добывается общинной торговлей (§1.2, обычный товар),
// Костяной нож/Серебряный оберег -- находка в кургане (AGridWorldManager::
// LootKurgan) -- артефакт-тир, не рыночный товар, отсюда самый высокий
// RarityWeight (редкость).
//
// Идемпотентен: ряд, уже присутствующий в таблице (найден по имени),
// пропускается с предупреждением, не перезаписывается.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=GatheringToolAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GatheringToolAppendCommandlet.generated.h"

UCLASS()
class UGatheringToolAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
