#include "ProjectHerbalistGameModeBase.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Types/HerbalistCoreTypes.h"

// Конструктор
AProjectHerbalistGameModeBase::AProjectHerbalistGameModeBase()
{
}

// BeginPlay
void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    using namespace HerbalistCore;

    // === БАЗОВОЕ СОСТОЯНИЕ ===
    FRealState A;
    A.Magnitude = 0.5f;

    A.Direction.Body = 0.8f;
    A.Direction.Mind = 0.2f;
    A.Direction.Spirit = 0.1f;
    A.Direction.Nature = 0.4f;

    FRealState B;
    B.Magnitude = 0.3f;

    B.Direction.Body = 0.3f;
    B.Direction.Mind = 0.7f;
    B.Direction.Spirit = 0.2f;
    B.Direction.Nature = 0.6f;

    // === КОНТЕКСТ ===
    FEnvironment Env;
    Env.Toxicity = 0.9f;
    Env.Fertility = 0.2f;
    Env.Moisture = 0.5f;

    FMemoryState Memory;
    Memory.AccumulatedDistortion = 0.9f;
    Memory.StabilityMemory = 0.3f;
    Memory.HistoryPurity = 0.1f;

    FIntent Intent;
    Intent.Coherence = 0.1f;

    FRngState Rng;

    // === ПРОГОН ===
    FRealState Result = Pipeline::ApplyMorok(A, B, Env, Memory, Intent, Rng);

    // === ЛОГИ ===
    UE_LOG(LogTemp, Warning, TEXT("Result Magnitude: %f"), Result.Magnitude);
    UE_LOG(LogTemp, Warning, TEXT("Distortion: %f"), Result.Meta.Distortion);
    UE_LOG(LogTemp, Warning, TEXT("Purity: %f"), Result.Meta.Purity);

    UE_LOG(LogTemp, Warning, TEXT("A.Mag: %f | B.Mag: %f"), A.Magnitude, B.Magnitude);
    UE_LOG(LogTemp, Warning, TEXT("Env.Toxicity: %f"), Env.Toxicity);
    UE_LOG(LogTemp, Warning, TEXT("Memory.Distortion: %f"), Memory.AccumulatedDistortion);
    UE_LOG(LogTemp, Warning, TEXT("Intent.Coherence: %f"), Intent.Coherence);

    // 👉 КРИТИЧНО: направление
    UE_LOG(LogTemp, Warning, TEXT("Dir Body: %f"), Result.Direction.Body);
    UE_LOG(LogTemp, Warning, TEXT("Dir Mind: %f"), Result.Direction.Mind);
    UE_LOG(LogTemp, Warning, TEXT("Dir Spirit: %f"), Result.Direction.Spirit);
    UE_LOG(LogTemp, Warning, TEXT("Dir Nature: %f"), Result.Direction.Nature);

    UE_LOG(LogTemp, Warning, TEXT("Game started"));
}