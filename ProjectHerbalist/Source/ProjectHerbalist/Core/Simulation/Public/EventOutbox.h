#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/World/WorldStateSubsystem.h"
#include "EventOutbox.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldChanged, const FWorldDelta&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, const FInventoryDelta&);

UCLASS()
class PROJECTHERBALIST_API UEventOutbox : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    FOnWorldChanged OnWorldChanged;
    FOnInventoryChanged OnInventoryChanged;

    void PublishWorldChanged(const FWorldDelta& Delta);
    void PublishInventoryChanged(const FInventoryDelta& Delta);

private:
    bool bIsPublishing = false;
};
