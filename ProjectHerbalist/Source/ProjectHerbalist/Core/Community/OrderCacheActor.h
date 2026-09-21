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
#include "OrderCacheActor.generated.h"

class AGridWorldManager;
class AHerbalistPlayerController;
class UStaticMeshComponent;
class USphereComponent;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AOrderCacheActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AOrderCacheActor();

    // Открывает окно заказов: у тайника из него можно отдать зелье.
    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

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
