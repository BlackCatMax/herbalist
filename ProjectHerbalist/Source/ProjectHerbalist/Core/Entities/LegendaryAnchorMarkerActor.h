// LegendaryAnchorMarkerActor.h
//
// Постоянный слабый маркер якорной клетки Легендарного ранга (Архетип 3,
// DESIGN_Entity_Actors_Art.md, 2026-09-06: "чтобы игрок мог найти её снова
// взглядом даже когда эффект неактивен"). Отдельный от ALegendaryEntityActor
// намеренно — тот существует только пока проявление активно
// (SyncManifestedEntityActor уничтожает его при спаде), этот живёт всегда,
// с момента сева якорей до конца сессии. Тот же класс решения, что уже
// APOI_Totem/APOI_Svetloyar/APOI_GoryuchKamen: спавнится сам
// AGridWorldManager::SeedLegendaryAnchors, меш/эффект — TODO на финальный арт.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LegendaryAnchorMarkerActor.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API ALegendaryAnchorMarkerActor : public AActor
{
    GENERATED_BODY()

public:
    ALegendaryAnchorMarkerActor();

    void Init(FName InEntityID, const FIntPoint& InGridCell);

    FName GetEntityID() const { return EntityID; }
    FIntPoint GetGridCell() const { return GridCell; }

protected:
    // TODO: заменить на финальный арт -- постоянный слабый маркер (не
    // полноценная модель существа), не хардкодит меш/материал.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MarkerMesh;

private:
    FName EntityID;
    FIntPoint GridCell = FIntPoint(-1, -1);
};
