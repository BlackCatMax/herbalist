// AlchemyTableActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlchemyTableActor.generated.h"

class UBoxComponent;
class UAlchemyTransferWidget;

UCLASS()
class PROJECTHERBALIST_API AAlchemyTableActor : public AActor
{
    GENERATED_BODY()

public:
    AAlchemyTableActor();
    void OnInteract(class AHerbalistPlayerController* PC);
    void SetGridCoords(const FIntPoint& InCoords) { GridCoords = InCoords; }
    FIntPoint GetGridCoords() const { return GridCoords; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UBoxComponent* InteractionBox;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UAlchemyTransferWidget> AlchemyWidgetClass;

    UPROPERTY()
    UAlchemyTransferWidget* AlchemyWidgetInstance = nullptr;

    FIntPoint GridCoords;
};