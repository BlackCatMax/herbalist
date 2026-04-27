// Core/Pipeline/AlchemyPipelineFacade.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Pipeline/AlchemySemantics.h"
#include "Core/Pipeline/AlchemyTypes.h"

class UIngredientRegistrySubsystem;

struct FAlchemyFacadeResult
{
    FRealState FinalState;
    EAlchemyOutcome Outcome = EAlchemyOutcome::Ash;
    // Дополнительно можно вернуть сырой результат физики и дельту,
    // но для нашего случая достаточно состояния.
};

class FAlchemyPipelineFacade
{
public:
    /**
     * Выполняет полный алхимический расчёт (семантика + физика + применение трансформаций)
     * без воздействия на мир.
     * @param Items ингредиенты с ID и состоянием
     * @param CellState текущее состояние клетки
     * @param Env окружение
     * @param Memory память клетки
     * @param GlobalDistortion глобальный уровень искажения
     * @param IngredientReg реестр ингредиентов (может быть nullptr)
     * @param BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift контекст биомного графа
     * @param Rng генератор случайных чисел (изменяется)
     * @return результат алхимии (состояние и исход)
     */
    static FAlchemyFacadeResult Execute(
        const TArray<FInventoryItem>& Items,
        const FRealState& CellState,
        const FEnvironment& Env,
        const FMemoryState& Memory,
        float GlobalDistortion,
        UIngredientRegistrySubsystem* IngredientReg,
        float BiomeMorokField,
        float BiomeZaryanaField,
        const FVector4& BiomeAxisDrift,
        FRngState& Rng
    );
};