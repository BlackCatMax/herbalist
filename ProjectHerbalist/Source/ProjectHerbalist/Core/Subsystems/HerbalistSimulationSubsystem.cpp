#include "Core/Subsystems/HerbalistSimulationSubsystem.h"

void UHerbalistSimulationSubsystem::EnqueueCommand(const FSimulationCommand& Command)
{
    CommandQueue.Add(Command);
}

TArray<FSimulationCommand> UHerbalistSimulationSubsystem::ConsumeQueuedCommands()
{
    TArray<FSimulationCommand> Out;
    Out = MoveTemp(CommandQueue);
    CommandQueue.Reset();
    return Out;
}
