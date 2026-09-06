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
//
// Архетип 2 (DESIGN_Entity_Actors_Art.md, 2026-09-06): "РЕДКОЕ, краткое
// появление силуэта... при пересечении порога Respect в любую сторону".
// Пороги (0.5 благословение / -0.3 порча) НЕ новые числа — те же самые,
// что уже гейтят bless/curse-нудж в UpdateEntityManifestations
// (GridWorldManagerEntities.cpp), переиспользованы напрямую, не
// продублированы отдельной парой настроек: силуэт должен появляться
// ровно тогда, когда меняется механический статус хозяина, не по
// случайно совпадающему, но независимому порогу.
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
    ALandmarkEntityActor();

    // nullptr, если хозяин уже не проявлен (клетка ManifestedEntityID
    // сменилась) или мир недоступен -- тот же принцип терпимости к
    // отсутствующим данным, что и у остального внепайплайнового слоя.
    const FEntityLandmark* GetLandmark() const;

    // Текущее состояние, не только момент пересечения -- Blueprint (или
    // тест) может опросить его напрямую (например, в BeginPlay, до
    // первого Tick), а не только реагировать на событие ниже. Тот же
    // гистерезис (PassesHysteresisThreshold), что уже держит bless/curse-
    // нудж в UpdateEntityManifestations -- отдельная копия состояния
    // здесь, не общая с ним: актёр и симуляция гейтят РАЗНЫЕ вещи (виден
    // ли силуэт vs применяется ли нудж), даже читая один Respect.
    UPROPERTY(BlueprintReadOnly, Category = "Landmark")
    bool bIsCurrentlyBlessed = false;

    UPROPERTY(BlueprintReadOnly, Category = "Landmark")
    bool bIsCurrentlyCursed = false;

protected:
    virtual void Tick(float DeltaTime) override;

    // TODO: подключить силуэт -- BlueprintImplementableEvent вместо
    // хардкода конкретного VFX/аниматика в C++, тем же принципом, что
    // уже OnThud у APOI_GoryuchKamen. bNowBlessed/bNowCursed отражают
    // состояние ПОСЛЕ пересечения, оба false одновременно означает выход
    // из зоны и благословения, и порчи разом (Respect вернулся в
    // нейтральную зону).
    UFUNCTION(BlueprintImplementableEvent, Category = "Landmark")
    void OnRespectThresholdCrossed(bool bNowBlessed, bool bNowCursed);
};
