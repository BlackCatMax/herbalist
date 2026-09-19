// Core/World/GridWorldManagerTimeDisplay.cpp
//
// Время в материалах (2026-09-16, этап 1б docs/research/
// DESIGN_Living_Vegetation_Research.md §2): часы -> плавные веса ->
// MPC_WorldStateFields. Формулы -- Core/Types/HerbalistTimeDisplay.h, здесь
// только чтение часов, запись в коллекцию и перемотка.

#include "Core/World/GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Types/HerbalistCalendar.h"
#include "Core/Types/HerbalistTimeDisplay.h"
#include "Core/Config/HerbalistSettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

FLinearColor AGridWorldManager::GetDayPhaseWeights() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    return HerbalistCore::TimeDisplay::DayPhaseWeights(GetTimeOfDay01(),
        Settings ? Settings->GameDayMinutes : 32.0f,
        Settings ? Settings->DayPhaseBlendMinutes : 1.0f);
}

float AGridWorldManager::GetSeasonUDW() const
{
    return HerbalistCore::TimeDisplay::SeasonUDW(GetSeason(), GetSeasonProgress01());
}

FLinearColor AGridWorldManager::GetSeasonWeights() const
{
    return HerbalistCore::TimeDisplay::SeasonWeights(GetSeasonUDW());
}

float AGridWorldManager::GetLeafDrop01() const
{
    return HerbalistCore::TimeDisplay::LeafDrop01(GetSeasonUDW());
}

float AGridWorldManager::GetLeafFall01() const
{
    return HerbalistCore::TimeDisplay::LeafFall01(GetSeasonUDW());
}

float AGridWorldManager::GetLeafLitter01() const
{
    return HerbalistCore::TimeDisplay::LeafLitter01(GetSeasonUDW());
}

float AGridWorldManager::GetMoonFull01() const
{
    return HerbalistCore::TimeDisplay::MoonFull01(GetMoonCycle01());
}

namespace
{
    const FName TimeOfDay01Name(TEXT("TimeOfDay01"));
    const FName DayPhaseWeightsName(TEXT("DayPhaseWeights"));
    const FName SeasonWeightsName(TEXT("SeasonWeights"));
    const FName SeasonUDWName(TEXT("SeasonUDW"));
    const FName LeafDrop01Name(TEXT("LeafDrop01"));
    const FName LeafFall01Name(TEXT("LeafFall01"));
    const FName LeafLitter01Name(TEXT("LeafLitter01"));
    const FName MoonFull01Name(TEXT("MoonFull01"));
    const FName Morok01Name(TEXT("Morok01"));
}

void AGridWorldManager::UpdateMorokDisplay(float DeltaSeconds)
{
    const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    int32 X = 0;
    int32 Y = 0;
    if (!Pawn || !WorldPositionToCell(Pawn->GetActorLocation(), X, Y))
    {
        return;
    }
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Target = FMath::Clamp(ComputePerceptionDistortion(X, Y), 0.0f, 1.0f);
    // Местное искажение для контроллера -- там, где игрок стоит, каждый кадр
    // (раньше -- только при осмотре и сборе): по нему подсказка пишет
    // «Морочники путают чувства» и PerceiveClass подменяет имена.
    if (AHerbalistPlayerController* HerbalistPC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        HerbalistPC->CurrentGlobalDistortion = Target;
    }
    MorokDisplay01 = HerbalistCore::TimeDisplay::SmoothToward(MorokDisplay01, Target, DeltaSeconds,
        Settings ? Settings->MorokDisplaySmoothingSeconds : 2.0f);
}

void AGridWorldManager::WriteTimeDisplayParametersFromSettings()
{
    // Настройка читается один раз на менеджер: битый путь иначе пытался бы
    // грузиться и предупреждать каждый кадр.
    if (!bTimeDisplayCollectionResolved)
    {
        bTimeDisplayCollectionResolved = true;
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        TimeDisplayCollectionCached = Settings ? Settings->TimeDisplayCollection.LoadSynchronous() : nullptr;
    }
    if (UMaterialParameterCollection* Collection = TimeDisplayCollectionCached.Get())
    {
        WriteTimeDisplayParameters(Collection);
    }
}

bool AGridWorldManager::WriteTimeDisplayParameters(UMaterialParameterCollection* Collection)
{
    UWorld* World = GetWorld();
    if (!Collection || !World)
    {
        return false;
    }

    // Прямо в экземпляр коллекции, а не через UKismetMaterialLibrary: тот на
    // отсутствующий параметр пишет предупреждение в лог PIE, а коллекция без
    // заведённых параметров (коммандлет не запускали) -- не ошибка игры.
    UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(Collection);
    if (!Instance)
    {
        return false;
    }

    bool bAllFound = true;
    bAllFound &= Instance->SetScalarParameterValue(TimeOfDay01Name, GetTimeOfDay01());
    bAllFound &= Instance->SetVectorParameterValue(DayPhaseWeightsName, GetDayPhaseWeights());
    bAllFound &= Instance->SetVectorParameterValue(SeasonWeightsName, GetSeasonWeights());
    bAllFound &= Instance->SetScalarParameterValue(SeasonUDWName, GetSeasonUDW());
    bAllFound &= Instance->SetScalarParameterValue(LeafDrop01Name, GetLeafDrop01());
    bAllFound &= Instance->SetScalarParameterValue(LeafFall01Name, GetLeafFall01());
    bAllFound &= Instance->SetScalarParameterValue(LeafLitter01Name, GetLeafLitter01());
    bAllFound &= Instance->SetScalarParameterValue(MoonFull01Name, GetMoonFull01());
    bAllFound &= Instance->SetScalarParameterValue(Morok01Name, GetMorokDisplay01());
    return bAllFound;
}

void AGridWorldManager::JumpGameClock(double NewClockSeconds)
{
    const double Target = FMath::Max(0.0, NewClockSeconds);
    const double Delta = Target - GameClockSeconds;
    if (Delta < 0.0)
    {
        // Назад -- как загрузка: обереги гаснут, простой чанков считается от
        // нового времени (GridWorldManagerSave.cpp), иначе будущие отметки
        // простоя давали бы спящим чанкам ноль прошедшего времени.
        ResetSessionOnlyWardTimers();
        SetGameClockSeconds(Target);
        GridInitGameClock = GameClockSeconds;
        ChunkLastSimulatedGameTime.Reset();
    }
    else
    {
        // Вперёд: спящие чанки догонят пропущенное сами при активации, а
        // активные считает только обычный шаг тика -- без этого клетки вокруг
        // игрока не видели бы перемотанных суток (ревью этапа 1б).
        SetGameClockSeconds(Target);
        if (Delta > KINDA_SMALL_NUMBER)
        {
            RegenerateCellParameters(static_cast<float>(Delta));
            for (const FIntPoint& Chunk : ActiveChunks)
            {
                ChunkLastSimulatedGameTime.FindOrAdd(Chunk) = GameClockSeconds;
            }
        }
    }

    UE_LOG(LogHerbalistWorld, Display, TEXT("[Time] Часы %.0f с: %d.%02d, %s (по лору %s), доля суток %.3f, луна %s"),
        GameClockSeconds, GetCalendarDay(), GetCalendarMonth(),
        *UEnum::GetValueAsString(GetSeason()), *UEnum::GetValueAsString(GetLoreSeason()),
        GetTimeOfDay01(), *UEnum::GetValueAsString(GetMoonPhase()));

    // То же, что уходит в материалы (MPC_WorldStateFields), и погода, которую
    // видит симуляция: проверка в PIE без открытия материала
    // (docs/verification/pie/04_World_State.md, «Живая растительность»).
    const FLinearColor Phases = GetDayPhaseWeights();
    const FLinearColor Seasons = GetSeasonWeights();
    UE_LOG(LogHerbalistWorld, Display,
        TEXT("[Time] Показ: сутки р/д/з/н %.2f/%.2f/%.2f/%.2f, сезоны в/л/о/з %.2f/%.2f/%.2f/%.2f, SeasonUDW %.3f, листва опала %.2f, листопад %.2f, подстилка %.2f, полнолуние %.2f; погода (%s) дождь %.2f снег %.2f ветер %.2f"),
        Phases.R, Phases.G, Phases.B, Phases.A, Seasons.R, Seasons.G, Seasons.B, Seasons.A,
        GetSeasonUDW(), GetLeafDrop01(), GetLeafFall01(), GetLeafLitter01(), GetMoonFull01(),
        bWeatherBridgeActive ? TEXT("UDW") : TEXT("шум"), GetRainIntensity(), GetSnowIntensity(), GetWindIntensity());
}
