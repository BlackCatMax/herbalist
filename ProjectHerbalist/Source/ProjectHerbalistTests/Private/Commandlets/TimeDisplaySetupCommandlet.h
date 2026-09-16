// TimeDisplaySetupCommandlet.h
//
// Заводит в MPC_WorldStateFields параметры времени для материалов (2026-09-16,
// этап 1б docs/research/DESIGN_Living_Vegetation_Research.md §2), если их
// нет: скаляры TimeOfDay01, SeasonUDW, LeafDrop01, LeafFall01, LeafLitter01,
// MoonFull01 и векторы
// DayPhaseWeights, SeasonWeights. Значения пишет AGridWorldManager каждый тик
// (WriteTimeDisplayParameters). Карты не трогает: путь к коллекции -- в
// Herbalist Settings (TimeDisplayCollection). Идемпотентен.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=TimeDisplaySetup
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "TimeDisplaySetupCommandlet.generated.h"

UCLASS()
class UTimeDisplaySetupCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
