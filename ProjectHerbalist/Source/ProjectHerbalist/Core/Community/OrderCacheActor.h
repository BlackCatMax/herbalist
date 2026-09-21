// OrderCacheActor.h
//
// Тайник для заказов (решение пользователя 2026-09-21: «тайником может быть
// любое место, я потом в мире размещу, на заказ не влияет»). Записки
// говорят «оставь в дупле у вербы» -- тайник и есть это дупло: зелье по
// заказу кладут в него, а не отдают у порога.
//
// Какой именно тайник -- для заказа неважно: подходит любой. Автор уровня
// ставит их сам, где захочет; симуляция их не ищет -- тайник регистрируется
// в AGridWorldManager сам из BeginPlay (тот же приём, что у ручного
// спавнера Низших).
//
// Пока на уровне нет ни одного тайника, отдавать можно где угодно, как
// раньше: иначе заказы встали бы до того, как их расставят
// (AGridWorldManager::IsDeliveryAllowedAt).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Interaction/HeldItemTarget.h"
#include "OrderCacheActor.generated.h"

class AGridWorldManager;
class AHerbalistPlayerController;
class UStaticMeshComponent;
class USphereComponent;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AOrderCacheActor : public AActor, public IInteractable, public IHeldItemTarget
{
    GENERATED_BODY()

public:
    AOrderCacheActor();

    // Пустой рукой в тайнике делать нечего: окно заказов ушло
    // (DESIGN_Diegetic_Interface.md, этап 3), осталось только отладочной
    // ToggleOrdersUI.
    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

    // Зелье из руки ложится в тайник и исполняет открытый заказ с ближайшим
    // сроком (решение пользователя 2026-09-21). Не зелье -- тайнику ни к чему.
    virtual bool ReceiveHeldItem(AHerbalistPlayerController* PC, int32 InventoryIndex) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // TODO: финальный арт -- дупло, камень с выемкой, корни у воды; меш
    // задаёт Blueprint-наследник, C++ его не хардкодит.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

private:
    UPROPERTY()
    TWeakObjectPtr<AGridWorldManager> RegisteredManager;
};
