// LandmarkEntityActor.h
//
// Основной ранг бестиария (§16.3, "Хозяева") как физический актор —
// 2026-08-30. В отличие от Низшего ранга, у Хозяев уже есть реальное,
// давно работающее число — Respect (FEntityLandmark::Respect,
// GridWorldManagerEntities.cpp), меняется через подношение (Apply-на-
// клетку-обиталище). GetLandmark() — единственная добавка сверх пустого
// маркера (в отличие от AmbientEntityActor): естественная точка для
// будущего визуального отклика (благословлён/осквернён) или экрана "об
// этом хозяине", раз сама точка привязки (WorldManagerRef+GridCell) уже
// у базового класса, а поиск (FindLandmarkAt) уже существует — не
// заводить новый канал ради одного геттера.
#pragma once

#include "CoreMinimal.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "LandmarkEntityActor.generated.h"

struct FEntityLandmark;

UCLASS()
class PROJECTHERBALIST_API ALandmarkEntityActor : public AHerbalistEntityActor
{
    GENERATED_BODY()

public:
    // nullptr, если хозяин уже не проявлен (клетка ManifestedEntityID
    // сменилась) или мир недоступен -- тот же принцип терпимости к
    // отсутствующим данным, что и у остального внепайплайнового слоя.
    const FEntityLandmark* GetLandmark() const;
};
