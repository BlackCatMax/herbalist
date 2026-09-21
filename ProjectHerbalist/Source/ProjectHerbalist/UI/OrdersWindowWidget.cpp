// OrdersWindowWidget.cpp
#include "UI/OrdersWindowWidget.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Community/OrderTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
    // Палитра окна -- та же, что у Травника (JournalLogWidget.cpp): одна
    // рука, одна бумага.
    const FLinearColor TitleColor(0.92f, 0.85f, 0.6f);
    const FLinearColor TextColor(0.85f, 0.83f, 0.78f);
    const FLinearColor MutedColor(0.55f, 0.55f, 0.5f);
    const FLinearColor SelectedColor(0.35f, 0.3f, 0.18f, 1.0f);
    const FLinearColor RowColor(0.12f, 0.11f, 0.09f, 1.0f);

    FString DescribePayment(const FOrderPayment& Payment)
    {
        if (Payment.ItemID.IsNone()) return TEXT("--");
        return Payment.Count > 1
            ? FString::Printf(TEXT("%s ×%d"), *Payment.ItemID.ToString(), Payment.Count)
            : Payment.ItemID.ToString();
    }
}

void UOrdersWindowRowProxy::HandleClicked()
{
    UOrdersWindowWidget* Window = Owner.Get();
    if (!Window) return;
    if (bIsOrder)
    {
        Window->SelectOrder(Value);
    }
    else
    {
        Window->SelectPotion(Value);
    }
}

void UOrdersWindowWidget::BindController(AHerbalistPlayerController* InController)
{
    if (Controller && Controller->InventoryComponent)
    {
        Controller->InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UOrdersWindowWidget::OnInventoryChanged);
    }
    Controller = InController;
    // Живое состояние, а не снимок: тот же приём, что у UInventoryWidget.
    if (Controller && Controller->InventoryComponent)
    {
        Controller->InventoryComponent->OnInventoryChanged.AddDynamic(this, &UOrdersWindowWidget::OnInventoryChanged);
    }
    if (OrderList)
    {
        RefreshDisplay();
    }
}

void UOrdersWindowWidget::NativeDestruct()
{
    if (Controller && Controller->InventoryComponent)
    {
        Controller->InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UOrdersWindowWidget::OnInventoryChanged);
    }
    Super::NativeDestruct();
}

void UOrdersWindowWidget::OnInventoryChanged()
{
    RefreshDisplay();
}

int32 UOrdersWindowWidget::ResolveSelectedPotionIndex() const
{
    if (!bHasSelectedPotion || !Controller || !Controller->InventoryComponent) return INDEX_NONE;
    // Общий поиск котомки, не точное сравнение: зелье тоже стареет, и выбор
    // слетал бы через секунду (ревью 2026-09-21, найдено на руке).
    return Controller->InventoryComponent->FindItemIndex(SelectedPotionItem);
}

void UOrdersWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
    RefreshDisplay();
}

void UOrdersWindowWidget::BuildLayout()
{
    if (!WidgetTree || OrderList) return;

    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OrdersBackdrop"));
    Backdrop->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f));
    Backdrop->SetHorizontalAlignment(HAlign_Center);
    Backdrop->SetVerticalAlignment(VAlign_Center);
    WidgetTree->RootWidget = Backdrop;

    USizeBox* Panel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OrdersPanel"));
    Panel->SetWidthOverride(900.0f);
    Panel->SetHeightOverride(560.0f);
    Backdrop->SetContent(Panel);

    UBorder* PanelBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OrdersPanelBackground"));
    PanelBackground->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.05f, 0.97f));
    PanelBackground->SetPadding(FMargin(20.0f));
    Panel->SetContent(PanelBackground);

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OrdersRoot"));
    PanelBackground->SetContent(Root);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OrdersTitle"));
    Title->SetText(FText::FromString(TEXT("Заказы")));
    Title->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 24));
    Title->SetColorAndOpacity(FSlateColor(TitleColor));
    Root->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

    // Две колонки: заказы шире, зелья уже.
    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("OrdersColumns"));
    UVerticalBoxSlot* ColumnsSlot = Root->AddChildToVerticalBox(Columns);
    ColumnsSlot->SetSize(ESlateSizeRule::Fill);

    auto MakeColumn = [this, Columns](const TCHAR* Name, const TCHAR* Header, float FillWeight, UVerticalBox*& OutList)
    {
        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(Name));
        UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(Column);
        FSlateChildSize ColumnSize(ESlateSizeRule::Fill);
        ColumnSize.Value = FillWeight;
        ColumnSlot->SetSize(ColumnSize);
        ColumnSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));

        UTextBlock* HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        HeaderText->SetText(FText::FromString(Header));
        HeaderText->SetColorAndOpacity(FSlateColor(MutedColor));
        Column->AddChildToVerticalBox(HeaderText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

        UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
        Column->AddChildToVerticalBox(Scroll)->SetSize(ESlateSizeRule::Fill);

        OutList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Scroll->AddChild(OutList);
    };
    UVerticalBox* Orders = nullptr;
    UVerticalBox* Potions = nullptr;
    MakeColumn(TEXT("OrdersColumn"), TEXT("Записки"), 2.0f, Orders);
    MakeColumn(TEXT("PotionsColumn"), TEXT("Зелья в котомке"), 1.0f, Potions);
    OrderList = Orders;
    PotionList = Potions;

    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OrdersStatus"));
    StatusText->SetColorAndOpacity(FSlateColor(TextColor));
    StatusText->SetAutoWrapText(true);
    Root->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 8.0f));

    UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("OrdersButtons"));
    Root->AddChildToVerticalBox(Buttons);

    auto MakeActionButton = [this, Buttons](const TCHAR* Label) -> UButton*
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Text->SetText(FText::FromString(Label));
        Button->SetContent(Text);
        Buttons->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
        return Button;
    };
    MakeActionButton(TEXT("Отдать"))->OnClicked.AddDynamic(this, &UOrdersWindowWidget::OnDeliverClicked);
    MakeActionButton(TEXT("Отказаться"))->OnClicked.AddDynamic(this, &UOrdersWindowWidget::OnRefuseClicked);
    MakeActionButton(TEXT("Закрыть"))->OnClicked.AddDynamic(this, &UOrdersWindowWidget::OnCloseClicked);
}

UButton* UOrdersWindowWidget::MakeRowButton(UVerticalBox* List, const FString& Label, int32 Value, bool bIsOrder, bool bSelected)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    Button->SetBackgroundColor(bSelected ? SelectedColor : RowColor);

    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Text->SetText(FText::FromString(Label));
    Text->SetColorAndOpacity(FSlateColor(TextColor));
    Text->SetAutoWrapText(true);
    Button->SetContent(Text);
    List->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

    UOrdersWindowRowProxy* Proxy = NewObject<UOrdersWindowRowProxy>(this);
    Proxy->Owner = this;
    Proxy->Value = Value;
    Proxy->bIsOrder = bIsOrder;
    Button->OnClicked.AddDynamic(Proxy, &UOrdersWindowRowProxy::HandleClicked);
    RowProxies.Add(Proxy);
    return Button;
}

void UOrdersWindowWidget::RefreshDisplay()
{
    if (!OrderList || !PotionList) return;

    OrderList->ClearChildren();
    PotionList->ClearChildren();
    RetiredRowProxies = MoveTemp(RowProxies);
    RowProxies.Reset();
    OrderRowCount = 0;
    PotionRowCount = 0;

    AGridWorldManager* Grid = Controller ? Controller->FindWorldManager() : nullptr;

    // --- Заказы ---
    if (Grid)
    {
        const double DayLength = Grid->GetOrderDayLengthSeconds();
        bool bSelectedStillOpen = false;
        for (const FActiveOrder& Order : Grid->GetActiveOrders())
        {
            const FOrderDefinition* Def = HerbalistOrders::FindOrderDefinition(Order.DefinitionID);
            const FString Note = Def ? Def->NoteText.ToString() : Order.DefinitionID.ToString();
            FString When;
            if (Order.State == EOrderState::Delivered)
            {
                When = TEXT("отдано, ждём утра");
            }
            else
            {
                const double DaysLeft = (Order.DeadlineClock - Grid->GetGameClockSeconds()) / FMath::Max(1.0, DayLength);
                When = FString::Printf(TEXT("осталось %.1f сут."), FMath::Max(0.0, DaysLeft));
                bSelectedStillOpen = bSelectedStillOpen || Order.Number == SelectedOrder;
            }
            const FString Pay = Def
                ? FString::Printf(TEXT("плата: %s, точно -- ещё %s"), *DescribePayment(Def->Payment), *DescribePayment(Def->ExactBonus))
                : FString();
            const FString Label = FString::Printf(TEXT("№%d  %s\n%s.  %s"), Order.Number, *Note, *When, *Pay);
            MakeRowButton(OrderList, Label, Order.Number, /*bIsOrder=*/true, Order.Number == SelectedOrder);
            ++OrderRowCount;
        }
        // Выбранный заказ исчез или уже отдан -- выбор снимаем.
        if (!bSelectedStillOpen)
        {
            SelectedOrder = INDEX_NONE;
        }
    }
    if (OrderRowCount == 0)
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Empty->SetText(FText::FromString(TEXT("Записок нет. Их приносят к порогу -- загляните утром.")));
        Empty->SetColorAndOpacity(FSlateColor(MutedColor));
        OrderList->AddChildToVerticalBox(Empty);
    }

    // --- Зелья ---
    UHerbalistInventoryComponent* Inventory = Controller ? Controller->InventoryComponent : nullptr;
    const int32 SelectedPotion = ResolveSelectedPotionIndex();
    if (Inventory)
    {
        const TArray<FInventoryItem>& Items = Inventory->GetItems();
        for (int32 Index = 0; Index < Items.Num(); ++Index)
        {
            if (!HerbalistOrders::IsDeliverable(Items[Index])) continue;
            // Имя -- то, что видит травник, а не настоящее (PerceiveClass):
            // окно не должно выдавать то, чего травник не знает.
            const FString Label = FString::Printf(TEXT("%s ×%d"), *Controller->GetPerceivedDisplayName(Items[Index]), Items[Index].Count);
            MakeRowButton(PotionList, Label, Index, /*bIsOrder=*/false, Index == SelectedPotion);
            ++PotionRowCount;
        }
    }
    // Выбранное зелье ушло из котомки -- выбор снимаем.
    if (SelectedPotion == INDEX_NONE)
    {
        bHasSelectedPotion = false;
    }
    if (PotionRowCount == 0)
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Empty->SetText(FText::FromString(TEXT("Сваренных зелий нет.")));
        Empty->SetColorAndOpacity(FSlateColor(MutedColor));
        PotionList->AddChildToVerticalBox(Empty);
    }

    // Подсказка о тайнике -- ровно то правило, которым проверяется отдача.
    if (PendingStatus.IsEmpty())
    {
        const bool bCaches = Grid && Grid->HasAnyOrderCache();
        SetStatus(bCaches
            ? TEXT("Выберите записку и зелье. Зелье кладут в тайник -- отдать можно, стоя у него.")
            : TEXT("Выберите записку и зелье."));
    }
    else
    {
        SetStatus(PendingStatus);
        PendingStatus.Reset();
    }
}

void UOrdersWindowWidget::SelectOrder(int32 OrderNumber)
{
    SelectedOrder = OrderNumber;
    RefreshDisplay();
}

void UOrdersWindowWidget::SelectPotion(int32 InventoryIndex)
{
    const UHerbalistInventoryComponent* Inventory = Controller ? Controller->InventoryComponent : nullptr;
    bHasSelectedPotion = Inventory && Inventory->GetItems().IsValidIndex(InventoryIndex);
    if (bHasSelectedPotion)
    {
        SelectedPotionItem = Inventory->GetItems()[InventoryIndex];
    }
    RefreshDisplay();
}

void UOrdersWindowWidget::DeliverSelected()
{
    if (!Controller) return;
    // Ячейку ищем заново по самому зелью: пока окно было открыто, котомка
    // могла сдвинуться.
    const int32 PotionIndex = ResolveSelectedPotionIndex();
    if (SelectedOrder == INDEX_NONE || !bHasSelectedPotion)
    {
        PendingStatus = TEXT("Сначала выберите записку и зелье.");
        RefreshDisplay();
        return;
    }
    if (PotionIndex == INDEX_NONE)
    {
        bHasSelectedPotion = false;
        PendingStatus = TEXT("Выбранного зелья в котомке уже нет -- выберите снова.");
        RefreshDisplay();
        return;
    }
    FText Reason;
    if (Controller->TryDeliverOrder(SelectedOrder, PotionIndex, Reason))
    {
        PendingStatus = FString::Printf(TEXT("Зелье по записке №%d оставлено. Исход -- наутро."), SelectedOrder);
        SelectedOrder = INDEX_NONE;
        bHasSelectedPotion = false;
    }
    else
    {
        PendingStatus = FString::Printf(TEXT("Не отдано: %s."), *Reason.ToString());
    }
    RefreshDisplay();
}

void UOrdersWindowWidget::RefuseSelected()
{
    if (!Controller) return;
    AGridWorldManager* Grid = Controller->FindWorldManager();
    if (!Grid || SelectedOrder == INDEX_NONE)
    {
        PendingStatus = TEXT("Сначала выберите записку.");
        RefreshDisplay();
        return;
    }
    PendingStatus = Grid->RefuseOrder(SelectedOrder)
        ? FString::Printf(TEXT("От записки №%d отказались."), SelectedOrder)
        : FString::Printf(TEXT("Записка №%d уже не открыта."), SelectedOrder);
    SelectedOrder = INDEX_NONE;
    RefreshDisplay();
}

void UOrdersWindowWidget::OnDeliverClicked()
{
    DeliverSelected();
}

void UOrdersWindowWidget::OnRefuseClicked()
{
    RefuseSelected();
}

void UOrdersWindowWidget::OnCloseClicked()
{
    if (Controller)
    {
        Controller->ToggleOrdersUI();
    }
}

void UOrdersWindowWidget::SetStatus(const FString& Text)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Text));
    }
}

FText UOrdersWindowWidget::GetStatusText() const
{
    return StatusText ? StatusText->GetText() : FText::GetEmpty();
}
