#pragma once

#include "CoreMinimal.h"
#include "IngredientTableRow.h"

class UDataTable;

struct PROJECTHERBALIST_API FIngredientRegistry
{
public:
    /** Инициализировать из DataTable. Вызывается один раз в UAlchemySubsystem::PostEngineInit. */
    static void Initialize(UDataTable* IngredientTable);

    /** Классифицировать ингредиент по имени. Никогда не возвращает ошибку — неизвестное возвращает Unknown (контракт §5). */
    static EIngredientClass Classify(FName IngredientName);

    /** Проверить, является ли ингредиент водой (используется PipelineWater.cpp). */
    static bool IsWater(FName IngredientName);

    /** Проверить, есть ли ингредиент в реестре (известен ли он системе). */
    static bool IsKnown(FName IngredientName);

    /** Получить количество зарегистрированных ингредиентов. */
    static int32 GetIngredientCount();

    /** Сбросить реестр (только для тестов). */
    static void Reset();

private:
    static TMap<FName, EIngredientClass> IngredientMap;
    static bool bIsInitialized;
};
