#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Interaction/HeldItemTarget.h"
#include "StorageContainer.generated.h"

UCLASS()
class PROJECTHERBALIST_API AStorageContainer : public AActor, public IInteractable, public IHeldItemTarget
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

    // Пустой рукой -- раскрыть содержимое пестерем перед камерой (этап 3,
    // решение пользователя 2026-09-21); повторно -- закрыть.
    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

    // Предмет из руки -- одна штука в хранилище, тем же TransferItemTo, что
    // у окна переноса. Нет места -- предмет остаётся в руке. Что станция
    // обрабатывает, решает её тик (StationType), а не приём: положить в
    // сушилку можно что угодно, сохнет только то, что портится.
    virtual bool ReceiveHeldItem(AHerbalistPlayerController* PC, int32 InventoryIndex) override;

    // Прежнее окно переноса -- только для отладки (консольная OpenStorageWindow).
    void OpenWindow(AHerbalistPlayerController* PC);

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