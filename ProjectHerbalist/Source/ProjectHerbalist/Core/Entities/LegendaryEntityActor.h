// LegendaryEntityActor.h
//
// Легендарный ранг бестиария (§16.4) как физический актор — 2026-08-30.
// У Легендарного ранга триггер (MorokField) живёт на уровне биом-графа, не
// на самом акторе (см. GridWorldManagerEntities.cpp).
//
// Архетип 3 (DESIGN_Entity_Actors_Art.md, 2026-09-06): "сильный, короткий
// VFX-всплеск... в момент срабатывания триггера (не постоянный)". Момент
// срабатывания триггера И есть момент спавна этого актора — SyncManifestedEntityActor
// (GridWorldManagerEntities.cpp) создаёт его именно тогда, когда
// Cell.ManifestedEntityID становится этим существом, и уничтожает, когда
// проявление спадает — тот же жизненный цикл, что уже описывает
// "манифестация", ничего отдельно отслеживать не нужно: BeginPlay этого
// актора буквально И есть момент триггера.
//
// "Постоянный слабый маркер на клетке-якоре, даже когда эффект неактивен"
// (та же часть архетипа) -- НЕ этот актор (он существует только пока
// проявление активно) -- отдельный, всегда живущий ALegendaryAnchorMarkerActor
// (см. LegendaryAnchorMarkerActor.h), спавнится один раз при
// SeedLegendaryAnchors, тем же приёмом, что уже POI-акторы в
// SeedPointsOfInterest.
#pragma once

#include "CoreMinimal.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "LegendaryEntityActor.generated.h"

struct FAcquiredArtifact;

UCLASS()
class PROJECTHERBALIST_API ALegendaryEntityActor : public AHerbalistEntityActor
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;

    // Баба-Яга (§4.4 флагман DESIGN_Entity_Actors_Art.md): "силуэт избушки
    // развёрнута иначе при честном/обманном исходе". Намеренно НЕ
    // хардкодит "Баба-Яга"/"Шапка-невидимка" в C++ -- generic-запрос по
    // ArtifactID, конкретная пара существо-артефакт остаётся знанием
    // контента (Blueprint-наследник этого актора для Бабы-Яги передаёт
    // сюда "Шапка-невидимка" сам). false, если артефакт ещё не добыт вовсе
    // (нет записи в AcquiredArtifacts) -- вызывающая сторона (Blueprint)
    // должна отдельно решить, что показывать ДО первой попытки.
    UFUNCTION(BlueprintPure, Category = "Legendary")
    bool WasAcquiredViaDeception(FName ArtifactID, bool& bOutFound) const;

    // Болотный царь (§4.4 флагман): "водная гладь... признак, что Царь
    // услышал и отвлёкся на приманку" -- вызывается АГридВорлдМенеджером
    // (TryLureSwampTsarWithPotion, GridWorldManagerArtifacts.cpp) на
    // акторе, чей GridCell совпадает с якорем Царя, при убедительной
    // (не обязательно успешной) приманке. Не переопределяется — общий
    // канал для любого будущего "существо среагировало, но не факт что
    // добыча состоялась" события, не специфичный для одного флагмана метод.
    UFUNCTION(BlueprintImplementableEvent, Category = "Legendary")
    void OnNoticedByBait();

protected:
    // TODO: подключить VFX-всплеск -- BlueprintImplementableEvent, тем же
    // принципом, что OnThud у APOI_GoryuchKamen/OnRespectThresholdCrossed у
    // ALandmarkEntityActor. Вызывается ровно один раз, из BeginPlay.
    UFUNCTION(BlueprintImplementableEvent, Category = "Legendary")
    void OnManifestationBurst();
};
