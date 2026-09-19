// Core/Community/OrderNoteActor.h
//
// Записка с заказом у порога (02_GDD/24_Orders_And_Repute.md §24.2). Тот же
// приём, что AMemoryFragmentActor: заглушка-меш, сфера взаимодействия.
// Прочитать -- текст на экран и в Травник, задаток в котомку, записка
// исчезает; заказ остаётся открытым до исполнения, отказа или срока.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "OrderNoteActor.generated.h"

class AGridWorldManager;
class AHerbalistPlayerController;
class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class PROJECTHERBALIST_API AOrderNoteActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AOrderNoteActor();

    void Init(int32 InOrderNumber, AGridWorldManager* InWorldManager);

    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

    int32 GetOrderNumber() const { return OrderNumber; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionSphere;

private:
    int32 OrderNumber = 0;

    UPROPERTY()
    AGridWorldManager* WorldManager = nullptr;

    bool bRead = false;
};
