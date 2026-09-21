// PesterItemActor.cpp
#include "Player/PesterItemActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/HeldItemComponent.h"
#include "Player/PesterComponent.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Components/StaticMeshComponent.h"

APesterItemActor::APesterItemActor()
{
    // Предмет в пестере должен ловить взгляд -- иначе его не взять, -- но
    // НЕ общие лучи мира (ревью 2026-09-21): пестерь висит в полуметре перед
    // камерой, и сбор, сведения о клетке, подсветка упирались бы в
    // собственную котомку. Поэтому ни один канал его не видит; взгляд
    // проверяет раскладку отдельно, прямым лучом по её формам
    // (UPesterComponent::FindItemUnderView), и только пока пестерь открыт.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void APesterItemActor::BindItem(const FInventoryItem& InItem, int32 InventoryIndex)
{
    Item = InItem;
    IndexHint = InventoryIndex;
}

void APesterItemActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    if (!PC || !PC->InventoryComponent || !PC->HeldItemComponent) return;

    // Ячейку ищем заново по самому предмету: пока пестерь открыт, котомка
    // живёт (распад, отрастание) -- номер мог сдвинуться.
    const int32 Index = PC->InventoryComponent->FindItemIndex(Item, IndexHint);
    if (Index == INDEX_NONE) return;

    // Сначала закрыть пестерь, потом взять: закрытие уничтожает раскладку, в
    // том числе и этот актор, -- после него к полям объекта не обращаемся.
    UHeldItemComponent* Hand = PC->HeldItemComponent;
    if (PC->PesterComponent)
    {
        PC->PesterComponent->Close();
    }
    Hand->TakeFromInventory(Index);
}
