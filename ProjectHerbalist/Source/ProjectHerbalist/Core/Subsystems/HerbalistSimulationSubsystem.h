#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "HerbalistSimulationSubsystem.generated.h"

UCLASS()
class PROJECTHERBALIST_API UHerbalistSimulationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Herbalist|Simulation")
    void EnqueueCommand(const FSimulationCommand& Command);

    UFUNCTION(BlueprintCallable, Category="Herbalist|Simulation")
    int32 GetQueuedCommandCount() const { return CommandQueue.Num(); }

    UFUNCTION(BlueprintCallable, Category="Herbalist|Simulation")
    TArray<FSimulationCommand> ConsumeQueuedCommands();

private:
    UPROPERTY(Transient)
    TArray<FSimulationCommand> CommandQueue;
};
