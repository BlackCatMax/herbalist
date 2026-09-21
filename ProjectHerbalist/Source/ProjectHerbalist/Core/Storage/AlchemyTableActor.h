// AlchemyTableActor.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCellCoord.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Interaction/HeldItemTarget.h"
#include "AlchemyTableActor.generated.h"

class UBoxComponent;
class UAlchemyTransferWidget;
class AHeldItemActor;
class AGridWorldManager;

// Чем кончилось «помешать» -- для тестов и лога.
enum class ECauldronStirResult : uint8
{
    Empty,              // в котле пусто
    BagFull,            // некуда принять зелье -- заложенное остаётся в котле
    NoWorld,            // нет менеджера мира
    Brewing,            // обычная варка поставлена, зелье придёт в котомку
    RitualProgressed,   // шаг ритуала принят, ритуал ждёт следующего часа
    RitualCompleted,    // ритуал завершён; результат в котомке, а без места -- ждёт в котле
    ResultTaken         // ждавший в котле результат ритуала зачерпнут в котомку
};

UCLASS()
class PROJECTHERBALIST_API AAlchemyTableActor : public AActor, public IInteractable, public IHeldItemTarget
{
    GENERATED_BODY()

public:
    AAlchemyTableActor();

    // Пустой рукой -- помешать (DESIGN_Diegetic_Interface.md, этап 3).
    virtual void OnInteract_Implementation(class AHerbalistPlayerController* PC) override;

    // Трава или вода в руке -- ложится в котёл, одна порция за жест. Порядок
    // закладки -- порядок жестов.
    virtual bool ReceiveHeldItem(AHerbalistPlayerController* PC, int32 InventoryIndex) override;

    // Помешать: если заложенное, вода и час совпали с шагом ритуала -- идёт
    // ритуал, иначе обычная варка тем же путём, что было у окна.
    ECauldronStirResult Stir(AHerbalistPlayerController* PC);

    // Прежнее окно варки -- только для отладки (консольная OpenCauldronWindow).
    void OpenWindow(AHerbalistPlayerController* PC);

    FIntPoint GetGridCoords() const { return GridCoords; }
    const TArray<FInventoryItem>& GetContents() const { return Contents; }
    bool HasReadyResult() const { return bHasReadyResult; }

    // Вместимость котла -- та же, что была у окна: одна вода и три травы.
    static constexpr int32 MaxWaterPortions = 1;
    static constexpr int32 MaxHerbPortions = 3;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UBoxComponent* InteractionBox;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UAlchemyTransferWidget> AlchemyWidgetClass;

    UPROPERTY()
    UAlchemyTransferWidget* AlchemyWidgetInstance = nullptr;

    FIntPoint GridCoords = HerbalistCore::InvalidCell();

private:
    void ClearContents();
    void SyncContentsVisual();
    void HandleBrewCompleted(const FInventoryItem& Produced, const FIntPoint& BrewCell);

    // Заложенное -- порции по одной, в порядке жестов.
    UPROPERTY()
    TArray<FInventoryItem> Contents;

    // Заглушки заложенного над котлом: видно, что в нём лежит.
    UPROPERTY()
    TArray<TObjectPtr<AHeldItemActor>> ContentActors;

    // Варки, чьё зелье ещё не пришло (приходит следующим тиком симуляции):
    // под каждую нужно своё свободное место в котомке -- урок окна варки
    // (ревью 2026-09-14), две варки при одном месте теряли второе зелье.
    int32 PendingBrews = 0;

    // Результат ритуала, которому не нашлось места в котомке.
    UPROPERTY()
    FInventoryItem ReadyResult;
    bool bHasReadyResult = false;
    TWeakObjectPtr<AGridWorldManager> BoundWorldManager;
    FDelegateHandle BrewCompletedHandle;
};
