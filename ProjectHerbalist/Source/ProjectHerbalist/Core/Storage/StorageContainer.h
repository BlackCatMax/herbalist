#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Interaction/Interactable.h"
#include "StorageContainer.generated.h"

UCLASS()
class PROJECTHERBALIST_API AStorageContainer : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AStorageContainer();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storage")
    UHerbalistInventoryComponent* InventoryComponent;

    // Пункт 4.4: настраиваемая вместимость
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage")
    int32 MaxSlots = 20;

    // Построено игроком (AGridWorldManager::SpawnHomeStorageContainer), а не
    // расставлено на карте (2026-09-14). Сейв различает их: построенное
    // пересоздаётся у стола, расставленное -- тот же актор уровня, из сейва
    // только содержимое.
    UPROPERTY()
    bool bIsHomeStorage = false;

    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

    // Для автотеста: задан ли класс окна переноса (без него окно не откроется).
    UClass* GetTransferWidgetClass() const;

    // Выгрузка World Partition (EndPlay с RemovedFromWorld): отдаёт содержимое
    // контейнера карты менеджеру сетки. Публично ради автотеста: в редакторском
    // мире акторы не инициализированы, и RouteEndPlay до EndPlay не доходит.
    void StashContentsForUnload();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<class UInventoryTransferWidget> TransferWidgetClass;

    UPROPERTY()
    class UInventoryTransferWidget* TransferWidgetInstance = nullptr;
};