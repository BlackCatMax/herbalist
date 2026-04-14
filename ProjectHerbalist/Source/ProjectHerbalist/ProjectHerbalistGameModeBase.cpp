#include "ProjectHerbalistGameModeBase.h"
#include "ProjectHerbalist.h"
#include "Core/World/HerbalistSimulation.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    FWorldState World;
    if (WorldConfig)
    {
        World.Env = WorldConfig->Environment;
        World.Memory = WorldConfig->InitialMemory;
        World.Intent = WorldConfig->Intent;
        World.CurrentState = WorldConfig->InitialBiomeState;

        // Оригинальные ресурсы из конфига
        FRealState OriginalA = WorldConfig->InputA;
        FRealState OriginalB = WorldConfig->InputB;

        const int32 Steps = 10;

        for (int32 i = 0; i < Steps; i++)
        {
            FRealState InputA = OriginalA;
            FRealState InputB = OriginalB;

            if (bEnableRandomResourceMutation)
            {
                // Создаём генератор случайных чисел на основе исходного seed + шаг, чтобы повторяемость
                FRandomStream Stream(Rng.Seed + i);

                // Мутация InputA
                InputA.Magnitude += Stream.FRandRange(-0.05f, 0.05f);
                InputA.Magnitude = FMath::Clamp(InputA.Magnitude, 0.0f, 1.0f);
                InputA.Direction.Body += Stream.FRandRange(-0.05f, 0.05f);
                InputA.Direction.Mind += Stream.FRandRange(-0.05f, 0.05f);
                InputA.Direction.Spirit += Stream.FRandRange(-0.05f, 0.05f);
                InputA.Direction.Nature += Stream.FRandRange(-0.05f, 0.05f);
                // Нормализуем направление
                float Len = FMath::Sqrt(
                    InputA.Direction.Body * InputA.Direction.Body +
                    InputA.Direction.Mind * InputA.Direction.Mind +
                    InputA.Direction.Spirit * InputA.Direction.Spirit +
                    InputA.Direction.Nature * InputA.Direction.Nature
                );
                if (Len > KINDA_SMALL_NUMBER)
                {
                    InputA.Direction.Body /= Len;
                    InputA.Direction.Mind /= Len;
                    InputA.Direction.Spirit /= Len;
                    InputA.Direction.Nature /= Len;
                }
                // Клиппинг мета-параметров (оставляем как есть, можно тоже мутировать)
                InputA.Meta.Distortion = FMath::Clamp(InputA.Meta.Distortion + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
                InputA.Meta.Stability = FMath::Clamp(InputA.Meta.Stability + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
                InputA.Meta.Purity = FMath::Clamp(InputA.Meta.Purity + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);

                // Аналогично для InputB
                InputB.Magnitude += Stream.FRandRange(-0.05f, 0.05f);
                InputB.Magnitude = FMath::Clamp(InputB.Magnitude, 0.0f, 1.0f);
                InputB.Direction.Body += Stream.FRandRange(-0.05f, 0.05f);
                InputB.Direction.Mind += Stream.FRandRange(-0.05f, 0.05f);
                InputB.Direction.Spirit += Stream.FRandRange(-0.05f, 0.05f);
                InputB.Direction.Nature += Stream.FRandRange(-0.05f, 0.05f);
                Len = FMath::Sqrt(
                    InputB.Direction.Body * InputB.Direction.Body +
                    InputB.Direction.Mind * InputB.Direction.Mind +
                    InputB.Direction.Spirit * InputB.Direction.Spirit +
                    InputB.Direction.Nature * InputB.Direction.Nature
                );
                if (Len > KINDA_SMALL_NUMBER)
                {
                    InputB.Direction.Body /= Len;
                    InputB.Direction.Mind /= Len;
                    InputB.Direction.Spirit /= Len;
                    InputB.Direction.Nature /= Len;
                }
                InputB.Meta.Distortion = FMath::Clamp(InputB.Meta.Distortion + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
                InputB.Meta.Stability = FMath::Clamp(InputB.Meta.Stability + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
                InputB.Meta.Purity = FMath::Clamp(InputB.Meta.Purity + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
            }

            TArray<FRealState> Inputs = { InputA, InputB };
            FRealState Result = HerbalistSimulation::UpdateWorld(World, Inputs, Rng);

            UE_LOG(LogHerbalist, Warning, TEXT("Step %d | ResultDist: %.3f | MemDist: %.3f"),
                i, Result.Meta.Distortion, World.Memory.AccumulatedDistortion);
        }
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("WorldConfig is NULL - using defaults"));
    }
}