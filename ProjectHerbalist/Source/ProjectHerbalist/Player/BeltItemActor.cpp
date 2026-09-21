// BeltItemActor.cpp
#include "Player/BeltItemActor.h"
#include "Player/BeltComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"

ABeltItemActor::ABeltItemActor()
{
    // Тот же приём, что у предмета пестеря: только запросы, ни один канал --
    // иначе сбор и сведения о клетке упирались бы в собственный пояс.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void ABeltItemActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    if (PC && PC->BeltComponent)
    {
        PC->BeltComponent->UseSlot(Slot);
    }
}
