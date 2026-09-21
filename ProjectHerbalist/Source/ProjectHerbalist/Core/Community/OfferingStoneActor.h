// OfferingStoneActor.h
//
// Камень-жертвенник общины (DESIGN_Diegetic_Interface.md, этап 3; решение
// пользователя 2026-09-21: «отдельный камень-жертвенник»). Подношение общине
// раньше было консольной OfferToCommunity без места; теперь предмет из руки
// кладут на камень -- тем же AGridWorldManager::OfferToCommunity, Молва
// меняется так же.
//
// Автор уровня ставит камень сам, у деревни; сколько камней -- неважно,
// община одна. Симуляция камни не ищет и не регистрирует: он нужен только
// как цель взгляда.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Interaction/HeldItemTarget.h"
#include "OfferingStoneActor.generated.h"

class AHerbalistPlayerController;
class UStaticMeshComponent;
class USphereComponent;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AOfferingStoneActor : public AActor, public IInteractable, public IHeldItemTarget
{
    GENERATED_BODY()

public:
    AOfferingStoneActor();

    // Пустой рукой на камне делать нечего -- только строка в лог.
    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

    // Любой предмет из руки -- одна штука в подношение общине.
    virtual bool ReceiveHeldItem(AHerbalistPlayerController* PC, int32 InventoryIndex) override;

protected:
    // TODO: финальный арт -- камень у околицы; меш задаёт Blueprint-наследник.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;
};
