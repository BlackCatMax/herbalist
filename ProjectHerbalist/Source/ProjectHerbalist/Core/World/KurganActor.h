// KurganActor.h
//
// Физический подбор находки в кургане (DESIGN_Brewing_Situations_And_
// Lore.md §4.3, DECISIONS_LOG.md, "Раунд «Анализ несоответствий лор/
// дизайн/код»", решение №5, 2026-09-06: "физический актор в мире
// (переиспользовать паттерн AMemoryFragmentActor)... подбирается вручную —
// не тихое появление в инвентаре. Держит принцип «мир читается ногами, не
// уведомлениями»"). До этой правки находка курганов была голым Exec
// (AHerbalistPlayerController::LootKurgan, снимал клетку игрока и грантил
// предмет тихо) — единственный "предмет в мире" проекта без физического
// представления, тогда как артефакты Легендарных/фрагменты памяти уже
// подбираются руками через AMemoryFragmentActor. Этот актор — тот же
// паттерн: спавнится сам AGridWorldManager::SeedKurganSites в момент сева,
// подбирается через уже существующий IInteractable/Interact().
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "KurganActor.generated.h"

class AGridWorldManager;
class AHerbalistPlayerController;
class UStaticMeshComponent;
class USphereComponent;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AKurganActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AKurganActor();

    void Init(AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY, FName InGrantedIngredientID);

    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

    FIntPoint GetGridCell() const { return FIntPoint(GridX, GridY); }

protected:
    // TODO: заменить на финальный арт -- травяная насыпь с просадкой
    // (DESIGN_POI_Art_And_LevelDesign.md §6), меш не хардкодится (тот же
    // принцип, что уже AShrineActor).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

private:
    int32 GridX = -1;
    int32 GridY = -1;
    FName GrantedIngredientID;
    bool bLooted = false;

    UPROPERTY()
    TObjectPtr<AGridWorldManager> WorldManager = nullptr;
};
