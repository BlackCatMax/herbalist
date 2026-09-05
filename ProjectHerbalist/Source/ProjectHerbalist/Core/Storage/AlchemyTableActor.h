// AlchemyTableActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "AlchemyTableActor.generated.h"

class UBoxComponent;
class UAlchemyTransferWidget;

UCLASS()
class PROJECTHERBALIST_API AAlchemyTableActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AAlchemyTableActor();
    virtual void OnInteract_Implementation(class AHerbalistPlayerController* PC) override;
    FIntPoint GetGridCoords() const { return GridCoords; }

protected:
    virtual void BeginPlay() override;
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
