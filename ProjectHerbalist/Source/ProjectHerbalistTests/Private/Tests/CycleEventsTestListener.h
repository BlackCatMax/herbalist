// CycleEventsTestListener.h
//
// Слушатель событий кругов времени и погоды для CycleEventsTest.cpp:
// динамические делегаты менеджера сетки привязываются только к UFUNCTION
// объекта, поэтому тесту нужен свой маленький UObject.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "CycleEventsTestListener.generated.h"

UCLASS()
class UCycleEventsTestListener : public UObject
{
    GENERATED_BODY()

public:
    TArray<int32> DayStarts;
    TArray<EDayPhase> DayPhases;
    TArray<EMoonPhase> MoonPhases;
    TArray<ESeason> Seasons;
    int32 WeatherEvents = 0;
    bool bLastRainy = false;
    // Порядок событий в одном тике: буква на событие.
    FString Order;

    UFUNCTION()
    void HandleGameDayStarted(int32 DayIndex) { DayStarts.Add(DayIndex); Order += TEXT("D"); }

    UFUNCTION()
    void HandleDayPhaseChanged(EDayPhase NewPhase, EDayPhase PreviousPhase) { DayPhases.Add(NewPhase); Order += TEXT("P"); }

    UFUNCTION()
    void HandleMoonPhaseChanged(EMoonPhase NewPhase, EMoonPhase PreviousPhase) { MoonPhases.Add(NewPhase); Order += TEXT("M"); }

    UFUNCTION()
    void HandleSeasonChanged(ESeason NewSeason, ESeason PreviousSeason) { Seasons.Add(NewSeason); Order += TEXT("S"); }

    UFUNCTION()
    void HandleWeatherChanged(bool bRainy, bool bWindy, bool bBlizzard) { ++WeatherEvents; bLastRainy = bRainy; Order += TEXT("W"); }
};
