// AHerbalistResourceActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "AHerbalistResourceActor.generated.h"

class UStaticMeshComponent;
class AGridWorldManager;
class USphereComponent;
class UParticleSystem;
class USoundBase;

UCLASS(BlueprintType, Blueprintable)
class PROJECTHERBALIST_API AHerbalistResourceActor : public AActor
{
    GENERATED_BODY()

public:
    AHerbalistResourceActor();
	
	FRealState GetBaseState() const { return BaseState; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    void Init(FName InIngredientID, const FText& InDisplayName, UStaticMesh* Mesh,
    const FRealState& InBaseState, const FVector& Location,
    AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY);

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    void Harvest();

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    FName GetIngredientID() const { return IngredientID; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    int32 GetGridX() const { return GridX; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    int32 GetGridY() const { return GridY; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    bool IsBeingHarvested() const { return bIsBeingHarvested; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    void SetIngredientID(FName NewID) { IngredientID = NewID; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    void SetGridPosition(int32 X, int32 Y) { GridX = X; GridY = Y; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    void SetWorldManager(AGridWorldManager* Manager) { WorldManager = Manager; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    void FindAndSetWorldManager();

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Resource")
    bool IsInHarvestRange(float MaxDistance) const;

protected:
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Herbalist|Effects")
    void OnHarvestStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Herbalist|Effects")
    void OnHarvestComplete();

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Effects")
    void StartDisappearAnimation();

    UFUNCTION()
    void OnDisappearAnimationFinished();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* HarvestSphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Resource")
    FName IngredientID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Resource")
    int32 GridX = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Resource")
    int32 GridY = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Resource")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Resource")
    FRealState BaseState;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Herbalist|Effects")
    UParticleSystem* HarvestParticleSystem;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Herbalist|Effects")
    USoundBase* HarvestSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Herbalist|Effects")
    float DisappearDuration = 0.1f;

    UPROPERTY()
    AGridWorldManager* WorldManager = nullptr;

private:
    bool bIsBeingHarvested = false;
    FTimerHandle DisappearTimerHandle;
};