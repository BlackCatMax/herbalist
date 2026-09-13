// Core/World/WaterRegionVolume.h
//
// Явно нарисованная форма воды (2026-09-02, прямой запрос пользователя):
// "делаем её как отдельный биом... я могу указывать, где будет вода, и она
// всегда превалирует над любым биомом... её вес всегда 1... биом воды,
// размещённый поверх других биомов, должен автоматически становиться тем
// типом воды, который для биома характерен". Тот же спавн-механизм
// (сплайн + point-in-polygon), что и у ABiomeRegionVolume -- наследуется
// напрямую, чтобы не дублировать UpdateCachedPoints/IsPointInside.
//
// Biome/MinResourcesPer100SquareMeters/MaxResourcesPer100SquareMeters/ResourceRegrowthTimeSeconds,
// унаследованные от родителя, здесь НЕ используются (водную клетку засевает
// аквапул водных растений в SpawnResourcesInCell, а плотность и отрастание
// берутся у земляного региона) -- остаются видимыми в Details как безвредный,
// не идеальный побочный эффект переиспользования готовой геометрии, не
// стоящий дублирования спланового кода ради чистоты панели.
//
// Регион воды -- единственный источник воды (2026-09-13, решение
// пользователя: пятна воды по плотности региона убраны). Клетка внутри формы
// получает bIsWater=true БЕЗУСЛОВНО (вес всегда 1), а вода берётся от УЖЕ
// определённых биомов клетки -- их дают обычные, "земляные"
// ABiomeRegionVolume, покрывающие ту же клетку: тип воды для сбора -- от
// доминирующего биома, состояние -- смесь воды биомов по долям BiomeWeights,
// и на стыке биомов вода смешивается (AGridWorldManager::RollWaterStateForCell).
// AWaterRegionVolume НЕ участвует в проходе InitializeCells по земляным
// регионам (явно исключается по типу в GridWorldManagerCore.cpp) -- иначе
// унаследованный дефолтный Biome=MixedForest ошибочно застолбил бы себе долю
// в Cell.BiomeWeights, подменяя собой земляной биом, а не просто заливая его
// водой поверх.
#pragma once

#include "CoreMinimal.h"
#include "Core/World/BiomeRegionVolume.h"
#include "WaterRegionVolume.generated.h"

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AWaterRegionVolume : public ABiomeRegionVolume
{
    GENERATED_BODY()
};
