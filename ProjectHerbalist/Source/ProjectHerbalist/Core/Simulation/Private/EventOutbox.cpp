#include "Core/Simulation/Public/EventOutbox.h"

void UEventOutbox::PublishWorldChanged(const FWorldDelta& Delta)
{
    if (bIsPublishing) return;
    bIsPublishing = true;
    OnWorldChanged.Broadcast(Delta);
    bIsPublishing = false;
}

void UEventOutbox::PublishInventoryChanged(const FInventoryDelta& Delta)
{
    if (bIsPublishing) return;
    bIsPublishing = true;
    OnInventoryChanged.Broadcast(Delta);
    bIsPublishing = false;
}
