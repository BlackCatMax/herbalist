// PesterItemActor.cpp
#include "Player/PesterItemActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/HeldItemComponent.h"
#include "Player/PesterComponent.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HerbalistLogChannels.h"

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

    // Раскрыто хранилище -- взять значит перенести штуку в котомку и в руку
    // (этап 3). Раскладка остаётся: из погреба берут не по одному заходу.
    UHerbalistInventoryComponent* Source = PC->PesterComponent ? PC->PesterComponent->GetSourceInventory() : PC->InventoryComponent;
    if (!Source) return;
    if (Source != PC->InventoryComponent)
    {
        const int32 SourceIndex = Source->FindItemIndex(Item, IndexHint);
        if (SourceIndex == INDEX_NONE) return;
        // Перенос перестроит раскладку и уничтожит этот актор -- всё нужное
        // берём в локальные до него.
        const FInventoryItem Taken = Source->GetItems()[SourceIndex];
        UHerbalistInventoryComponent* Bag = PC->InventoryComponent;
        UHeldItemComponent* HandComponent = PC->HeldItemComponent;
        if (!Source->TransferItemTo(SourceIndex, Bag))
        {
            UE_LOG(LogHerbalistPlayer, Log, TEXT("Pester: котомка полна -- '%s' остался в хранилище"), *Taken.IngredientID.ToString());
            return;
        }
        const int32 BagIndex = Bag->FindItemIndex(Taken);
        if (BagIndex != INDEX_NONE)
        {
            HandComponent->TakeFromInventory(BagIndex);
        }
        return;
    }

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
