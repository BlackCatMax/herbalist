// OrdersWindowWidget.h
//
// Окно заказов (2026-09-21) вместо трёх консольных команд ListOrders /
// DeliverOrder / RefuseOrder: слева -- открытые заказы (номер, записка,
// срок, плата), справа -- зелья из котомки, внизу -- «Отдать» и
// «Отказаться». Выбрал заказ, выбрал зелье, отдал.
//
// Строит дерево целиком в C++, как UJournalLogWidget: ни одного BindWidget,
// никакого .uasset -- CreateWidget с голым StaticClass() уже работает.
//
// Отдача идёт тем же путём, что и команда (AHerbalistPlayerController::
// TryDeliverOrder): только сваренное зелье и только у тайника, если тайники
// на уровне есть. Причина отказа выводится в строку состояния окна.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "OrdersWindowWidget.generated.h"

class AHerbalistPlayerController;
class UVerticalBox;
class UTextBlock;
class UButton;

// Кнопка строки списка не несёт своего номера (OnClicked без параметров) --
// номер держит этот маленький посредник, по одному на строку.
UCLASS()
class PROJECTHERBALIST_API UOrdersWindowRowProxy : public UObject
{
    GENERATED_BODY()

public:
    TWeakObjectPtr<class UOrdersWindowWidget> Owner;
    int32 Value = INDEX_NONE;
    bool bIsOrder = true;

    UFUNCTION()
    void HandleClicked();
};

UCLASS()
class PROJECTHERBALIST_API UOrdersWindowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void BindController(AHerbalistPlayerController* InController);

    // Перечитать заказы и котомку. Зовётся после каждого действия и при
    // открытии -- окно показывает живое состояние, а не снимок.
    void RefreshDisplay();

    void SelectOrder(int32 OrderNumber);
    void SelectPotion(int32 InventoryIndex);

    // Для тестов и для кнопок: что выбрано и сколько строк построено.
    int32 GetSelectedOrder() const { return SelectedOrder; }
    int32 GetSelectedPotion() const { return ResolveSelectedPotionIndex(); }
    int32 GetOrderRowCount() const { return OrderRowCount; }
    int32 GetPotionRowCount() const { return PotionRowCount; }
    FText GetStatusText() const;

    // То же, что нажатия кнопок, -- тестам не нужно кликать по Slate.
    void DeliverSelected();
    void RefuseSelected();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    // Котомка поменялась, пока окно открыто (отрастание, трата, стопки) --
    // списки перестраиваются, а выбранное зелье ищется заново по нему самому,
    // не по номеру ячейки (ревью 2026-09-21).
    UFUNCTION()
    void OnInventoryChanged();

    // Где сейчас лежит выбранное зелье. INDEX_NONE -- его больше нет.
    int32 ResolveSelectedPotionIndex() const;
    void BuildLayout();
    void SetStatus(const FString& Text);
    UButton* MakeRowButton(UVerticalBox* List, const FString& Label, int32 Value, bool bIsOrder, bool bSelected);

    UFUNCTION()
    void OnDeliverClicked();
    UFUNCTION()
    void OnRefuseClicked();
    UFUNCTION()
    void OnCloseClicked();

    UPROPERTY()
    TObjectPtr<AHerbalistPlayerController> Controller = nullptr;

    UPROPERTY()
    TObjectPtr<UVerticalBox> OrderList = nullptr;

    UPROPERTY()
    TObjectPtr<UVerticalBox> PotionList = nullptr;

    UPROPERTY()
    TObjectPtr<UTextBlock> StatusText = nullptr;

    // Посредники кнопок живут, пока живёт окно (UPROPERTY -- чтобы их не
    // собрал GC, пока на них смотрит делегат кнопки).
    UPROPERTY()
    TArray<TObjectPtr<UOrdersWindowRowProxy>> RowProxies;

    // Посредники прошлой перестройки: клик по строке сам зовёт перестройку,
    // и нажатый посредник должен дожить до конца своего обработчика. Держим
    // их ещё один цикл, а не сбрасываем посреди его же вызова.
    UPROPERTY()
    TArray<TObjectPtr<UOrdersWindowRowProxy>> RetiredRowProxies;

    // Выбранное зелье -- сам предмет, не номер ячейки: номер сдвигается,
    // когда из котомки уходит что-то выше (ревью 2026-09-21: иначе можно
    // было тихо отдать не то зелье, а заказ сверяется по настоящему).
    bool bHasSelectedPotion = false;
    FInventoryItem SelectedPotionItem;

    int32 SelectedOrder = INDEX_NONE;
    int32 OrderRowCount = 0;
    int32 PotionRowCount = 0;
    FString PendingStatus;
};
