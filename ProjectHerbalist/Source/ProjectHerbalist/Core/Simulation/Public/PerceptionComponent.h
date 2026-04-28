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

    // Обычный C++ метод
    const FPerceivedWorld& GetPerceivedWorld() const { return CachedPerceivedWorld; }

protected:
    UPROPERTY()
    FPerceivedWorld CachedPerceivedWorld;
};