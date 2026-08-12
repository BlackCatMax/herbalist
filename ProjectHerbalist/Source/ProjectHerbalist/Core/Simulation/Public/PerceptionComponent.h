// Core/Simulation/Public/PerceptionComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Simulation/Public/PerceivedTypes.h"
#include "PerceptionComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UPerceptionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPerceptionComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Обычные C++ методы
    const FPerceivedWorld& GetPerceivedWorld() const { return CachedPerceivedWorld; }
    const FPerceivedInventory& GetPerceivedInventory() const { return CachedPerceivedInventory; }

protected:
    UPROPERTY()
    FPerceivedWorld CachedPerceivedWorld;

    // FPerceivedInventory — обычная C++ структура (не USTRUCT), UPROPERTY не нужен;
    // внутри только FName/FRealState/int32/float/bool, GC-отслеживаемых указателей нет.
    FPerceivedInventory CachedPerceivedInventory;
};