// PesterItemActor.h
//
// Предмет в раскрытом пестере (DESIGN_Diegetic_Interface.md, этап 2,
// 2026-09-21). Та же заглушка, что у руки (AHeldItemActor): форма по классу,
// цвет по ведущей оси воспринятого состояния; стопка побольше -- заглушка
// крупнее («горсть», «полный мешочек»).
//
// В отличие от предмета в руке, на него можно посмотреть и взять: он
// ловит трассу взгляда, подсвечивается и по клавише взаимодействия ложится
// в руку. Пестерь при этом закрывается -- предмет теперь в руке.
#pragma once

#include "CoreMinimal.h"
#include "Player/HeldItemActor.h"
#include "Core/Interaction/Interactable.h"
#include "PesterItemActor.generated.h"

UCLASS()
class PROJECTHERBALIST_API APesterItemActor : public AHeldItemActor, public IInteractable
{
    GENERATED_BODY()

public:
    APesterItemActor();

    // Какой предмет котомки здесь лежит: сам предмет (для поиска) и ячейка,
    // где он был при раскладке (первая догадка).
    void BindItem(const FInventoryItem& Item, int32 InventoryIndex);
    const FInventoryItem& GetItem() const { return Item; }
    int32 GetIndexHint() const { return IndexHint; }

    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

private:
    FInventoryItem Item;
    int32 IndexHint = INDEX_NONE;
};
