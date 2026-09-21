// BeltItemActor.h
//
// Предмет на поясе (DESIGN_Diegetic_Interface.md, этап 4, 2026-09-21): та же
// заглушка, что у руки и пестеря. Виден, когда смотришь вниз; пустой рукой
// по нему -- снять в руку (инструмент, контейнер, оберег) или развязать и
// завязать семенной мешочек.
//
// Как и предмет пестеря, мир его не видит: ни один канал не отвечает, взгляд
// спрашивает пояс отдельно (UBeltComponent::FindItemUnderView).
#pragma once

#include "CoreMinimal.h"
#include "Player/HeldItemActor.h"
#include "Core/Interaction/Interactable.h"
#include "BeltItemActor.generated.h"

UENUM(BlueprintType)
enum class EBeltSlot : uint8
{
    Tool,       // серп, нож
    Container,  // корзина, мешок, туёс
    Ward,       // оберег
    SeedPouch,  // семенной мешочек: развязан -- сбор на семена
    Count UMETA(Hidden)
};

UCLASS()
class PROJECTHERBALIST_API ABeltItemActor : public AHeldItemActor, public IInteractable
{
    GENERATED_BODY()

public:
    ABeltItemActor();

    void SetSlot(EBeltSlot InSlot) { Slot = InSlot; }
    EBeltSlot GetSlot() const { return Slot; }

    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

private:
    EBeltSlot Slot = EBeltSlot::Tool;
};
