// MemoryFragmentActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MemoryFragmentActor.generated.h"

class AGridWorldManager;
class AHerbalistPlayerController;
class UStaticMeshComponent;
class USphereComponent;

// Заспавненный в мире фрагмент памяти Заряны — временное проявление
// (светящийся сгусток/видение), не предмет инвентаря. Исчезает, если не
// собрать за отведённое время. Взаимодействие — тем же Interact(), которым
// уже открываются AAlchemyTableActor/AStorageContainer (см.
// AHerbalistPlayerController::Interact).
UCLASS()
class PROJECTHERBALIST_API AMemoryFragmentActor : public AActor
{
    GENERATED_BODY()

public:
    AMemoryFragmentActor();

    void Init(FName InDefinitionID, bool bInIsFalse, float InLifetimeSeconds,
        AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY);

    void OnInteract(AHerbalistPlayerController* PC);

    FName GetDefinitionID() const { return DefinitionID; }
    bool IsFalse() const { return bIsFalse; }

protected:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionSphere;

private:
    FName DefinitionID;
    bool bIsFalse = false;
    float RemainingLifetime = 0.0f;
    int32 GridX = -1;
    int32 GridY = -1;

    UPROPERTY()
    AGridWorldManager* WorldManager = nullptr;

    bool bCollected = false;
};
