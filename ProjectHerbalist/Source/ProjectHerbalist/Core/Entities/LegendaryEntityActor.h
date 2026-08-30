// LegendaryEntityActor.h
//
// Легендарный ранг бестиария (§16.4) как физический актор — 2026-08-30.
// Пустой маркерный под-класс, тем же принципом, что AmbientEntityActor: у
// Легендарного ранга триггер (MorokField) живёт на уровне биом-графа, не на
// самом акторе (см. GridWorldManagerEntities.cpp) -- собственных данных,
// которые стоило бы открыть геттером уже сейчас, как у ALandmarkEntityActor::
// GetLandmark(), пока нет.
#pragma once

#include "CoreMinimal.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "LegendaryEntityActor.generated.h"

UCLASS()
class PROJECTHERBALIST_API ALegendaryEntityActor : public AHerbalistEntityActor
{
    GENERATED_BODY()
};
