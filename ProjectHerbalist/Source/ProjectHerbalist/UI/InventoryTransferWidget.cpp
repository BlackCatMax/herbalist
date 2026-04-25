#include "UI/InventoryTransferWidget.h"
#include "UI/InventoryWidget.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Components/VerticalBox.h"

void UInventoryTransferWidget::BindInventories(UHerbalistInventoryComponent* InLeftInventory, UHerbalistInventoryComponent* InRightInventory)
{
    if (!InventoryWidgetClass) return;

    if (LeftInventoryContainer)
    {
        LeftInventoryContainer->ClearChildren();
        LeftInventoryWidget = CreateWidget<UInventoryWidget>(GetWorld(), InventoryWidgetClass);
        if (LeftInventoryWidget && InLeftInventory)
        {
            LeftInventoryWidget->BindInventory(InLeftInventory);
            LeftInventoryContainer->AddChildToVerticalBox(LeftInventoryWidget);
        }
    }

    if (RightInventoryContainer)
    {
        RightInventoryContainer->ClearChildren();
        RightInventoryWidget = CreateWidget<UInventoryWidget>(GetWorld(), InventoryWidgetClass);
        if (RightInventoryWidget && InRightInventory)
        {
            RightInventoryWidget->BindInventory(InRightInventory);
            RightInventoryContainer->AddChildToVerticalBox(RightInventoryWidget);
        }
    }
}

UHerbalistInventoryComponent* UInventoryTransferWidget::GetLeftInventory() const
{
    return LeftInventoryWidget ? LeftInventoryWidget->GetInventoryComponent() : nullptr;
}

UHerbalistInventoryComponent* UInventoryTransferWidget::GetRightInventory() const
{
    return RightInventoryWidget ? RightInventoryWidget->GetInventoryComponent() : nullptr;
}
