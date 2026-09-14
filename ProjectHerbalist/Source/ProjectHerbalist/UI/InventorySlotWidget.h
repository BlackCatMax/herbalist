// InventorySlotWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "InventorySlotWidget.generated.h"

class UImage;
class UTextBlock;
class UItemTooltipWidget;
struct FPerceivedInventory;

UCLASS()
class PROJECTHERBALIST_API UInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory);
    // Refresh() удалена 2026-09-02 (чистка мёртвого кода) — тонкая обёртка над
    // приватным UpdateDisplay(), не звалась ниоткуда: слот перерисовывается
    // через InitializeSlot при перестроении списка (UInventoryWidget).

    // Публично только для теста на устойчивость к дрейфу State (аудит
    // 2026-09-05, см. подробный комментарий у FindRealIndex() в .cpp).
    int32 FindRealIndexForTest() const { return FindRealIndex(); }
    bool TryGetPerceivedItemForTest(FInventoryItem& OutItem) const { return TryGetPerceivedItem(OutItem); }
    FString GetProcessStatusForTest() const { return BuildProcessStatus(); }

    // Искажение одного предмета той же ComputePerceivedInventory, что у
    // восприятия менеджера: шум детерминирован от предмета и ясности.
    static FInventoryItem PerceiveSingleItem(const FInventoryItem& Item, float Clarity);

    // Искажённая копия предмета Inventory[RealIndex] для имени и подсказки.
    // Восприятие менеджера считает только сумку игрока (контейнер 0,
    // FSnapshotService::CaptureInventory) -- раньше слот ЛЮБОГО инвентаря брал
    // оттуда предмет с тем же номером строки, и в окне хранилища или станции
    // подсказка показывала числа чужого предмета из сумки (2026-09-14). Сумка
    // берёт кэш, если под этим номером всё ещё тот же предмет; остальное
    // искажается на месте той же ComputePerceivedInventory -- шум
    // детерминирован от предмета, результат тот же, что дал бы кэш.
    static bool ResolvePerceivedItem(const UHerbalistInventoryComponent* Inventory, int32 RealIndex,
        const UHerbalistInventoryComponent* PlayerInventory, const FPerceivedInventory* PlayerPerceived,
        float Clarity, FInventoryItem& OutItem);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    UImage* ItemIcon;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ItemNameText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* CountText;

    UPROPERTY(EditDefaultsOnly, Category = "Tooltip")
    TSubclassOf<UItemTooltipWidget> TooltipWidgetClass;

private:
    int32 FindRealIndex() const;

    // Искажённая (S_perceived) версия предмета этого слота -- см.
    // ResolvePerceivedItem. false, если предмета в инвентаре больше нет.
    bool TryGetPerceivedItem(FInventoryItem& OutItem) const;

    // Ясность восприятия менеджера; 0 без контроллера (автотесты).
    float GetPerceptionClarity() const;

    // Искажённый предмет для имени и подсказки. Предмета под слотом уже нет
    // (окно не успело пересобраться) -- искажается снимок слота: настоящее
    // состояние игроку не показывается.
    FInventoryItem GetPerceivedForDisplay() const;

    // Строка процессов станций настоящего предмета (GetItemProcessStatus):
    // снимок слота стоит, пока идут таймеры, OnInventoryChanged они не шлют.
    FString BuildProcessStatus() const;

    int32 SlotIndex = -1;

    UPROPERTY()
    UHerbalistInventoryComponent* InventoryComponent = nullptr;

    void UpdateDisplay();
    bool TryMoveToOtherInventory();

    FInventoryItem CachedItem;

    UPROPERTY()
    UItemTooltipWidget* ActiveTooltip;
};