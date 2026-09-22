// Core/World/Sky/UltraDynamicSkyBridge.cpp

#include "Core/World/Sky/UltraDynamicSkyBridge.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Types/HerbalistCalendar.h"
#include "Core/Config/HerbalistSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/UnrealType.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

namespace UltraDynamicSkyNames
{
    // UDS
    const FName TimeOfDay(TEXT("Time of Day"));
    const FName AnimateTimeOfDay(TEXT("Animate Time of Day"));
    const FName UseSystemTime(TEXT("Use System Time"));
    const FName Month(TEXT("Month"));
    const FName Day(TEXT("Day"));
    const FName Year(TEXT("Year"));
    const FName DawnTime(TEXT("Dawn Time"));
    const FName DuskTime(TEXT("Dusk Time"));
    const FName SetApplyDaylightSavingsTime(TEXT("Set Apply Daylight Savings Time"));
    const FName CreateStateForSaving(TEXT("Create UDS and UDW State for Saving"));
    const FName ApplySavedState(TEXT("Apply Saved UDS and UDW State"));
    const FName HardResetCache(TEXT("Hard Reset Cache"));
    // UDW
    const FName SeasonMode(TEXT("Season Mode"));
    const FName MeteorologicalSeasons(TEXT("Meteorological Seasons"));
    const FName SeasonDayOffset(TEXT("Season Day Offset"));
    const FName GlobalWeatherState(TEXT("Global Weather State"));
    // UDS_Weather_Settings (объект состояния погоды)
    const FName Rain(TEXT("Rain"));
    const FName Snow(TEXT("Snow"));
    const FName WindIntensity(TEXT("Wind Intensity"));
    const FName Fog(TEXT("Fog"));
    // Структура сейва: имена полей Blueprint-структуры с GUID-хвостом,
    // сверяются по началу. Объёмы погоды -- акторы уровня.
    const TCHAR* StateWovActorsPrefix = TEXT("WOVActors_");
    const TCHAR* StateWovStatesPrefix = TEXT("WOVStates_");
    const TCHAR* StateStructName = TEXT("UDS_and_UDW_State");

    // UDS_SeasonMode: 0 -- Use UDS Date, 1 -- Manual Setting.
    constexpr uint8 SeasonModeUseUdsDate = 0;
}

namespace
{
    template <typename TProperty>
    TProperty* FindUdsProperty(const UObject* Object, FName Name)
    {
        return Object ? FindFProperty<TProperty>(Object->GetClass(), Name) : nullptr;
    }

    bool SetUdsDouble(UObject* Object, FName Name, double Value)
    {
        if (FDoubleProperty* Property = FindUdsProperty<FDoubleProperty>(Object, Name))
        {
            Property->SetPropertyValue_InContainer(Object, Value);
            return true;
        }
        return false;
    }

    bool GetUdsDouble(const UObject* Object, FName Name, double& OutValue)
    {
        if (const FDoubleProperty* Property = FindUdsProperty<FDoubleProperty>(Object, Name))
        {
            OutValue = Property->GetPropertyValue_InContainer(Object);
            return true;
        }
        return false;
    }

    bool SetUdsBool(UObject* Object, FName Name, bool bValue)
    {
        if (FBoolProperty* Property = FindUdsProperty<FBoolProperty>(Object, Name))
        {
            Property->SetPropertyValue_InContainer(Object, bValue);
            return true;
        }
        return false;
    }

    bool SetUdsInt(UObject* Object, FName Name, int32 Value)
    {
        if (FIntProperty* Property = FindUdsProperty<FIntProperty>(Object, Name))
        {
            Property->SetPropertyValue_InContainer(Object, Value);
            return true;
        }
        return false;
    }

    bool SetUdsByte(UObject* Object, FName Name, uint8 Value)
    {
        if (FByteProperty* Property = FindUdsProperty<FByteProperty>(Object, Name))
        {
            Property->SetPropertyValue_InContainer(Object, Value);
            return true;
        }
        return false;
    }

    // Буфер параметров Blueprint-функции: инициализирует и разрушает значения
    // параметров, чтобы строки, массивы и структуры внутри не текли.
    struct FUdsCallParams
    {
        UFunction* Function = nullptr;
        TArray<uint8> Buffer;

        explicit FUdsCallParams(UFunction* InFunction)
            : Function(InFunction)
        {
            Buffer.SetNumZeroed(Function->ParmsSize);
            for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
            {
                It->InitializeValue_InContainer(Buffer.GetData());
            }
        }

        FUdsCallParams(const FUdsCallParams&) = delete;
        FUdsCallParams& operator=(const FUdsCallParams&) = delete;

        ~FUdsCallParams()
        {
            for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
            {
                It->DestroyValue_InContainer(Buffer.GetData());
            }
        }

        // Параметр-структура сейва UDS/UDW -- по имени типа, а не первый
        // попавшийся: у функции может появиться ещё один параметр-структура.
        FStructProperty* FindStateParam() const
        {
            for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
            {
                FStructProperty* Struct = CastField<FStructProperty>(*It);
                if (Struct && Struct->Struct && Struct->Struct->GetName().StartsWith(UltraDynamicSkyNames::StateStructName))
                {
                    return Struct;
                }
            }
            return nullptr;
        }

        FBoolProperty* FindBoolParam() const
        {
            for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
            {
                if (FBoolProperty* Bool = CastField<FBoolProperty>(*It))
                {
                    return Bool;
                }
            }
            return nullptr;
        }
    };

    UFunction* FindUdsFunction(UObject* Object, FName Name)
    {
        return Object ? Object->FindFunction(Name) : nullptr;
    }

    // Очищает в значении структуры сейва массивы ссылок на объёмы погоды.
    void ClearWeatherOverrideVolumes(const FStructProperty* StateParam, void* Container)
    {
        void* StateValue = StateParam->ContainerPtrToValuePtr<void>(Container);
        for (TFieldIterator<FProperty> It(StateParam->Struct); It; ++It)
        {
            const FString Name = It->GetName();
            if (Name.StartsWith(UltraDynamicSkyNames::StateWovActorsPrefix) || Name.StartsWith(UltraDynamicSkyNames::StateWovStatesPrefix))
            {
                if (const FArrayProperty* Array = CastField<FArrayProperty>(*It))
                {
                    FScriptArrayHelper(Array, Array->ContainerPtrToValuePtr<void>(StateValue)).EmptyValues();
                }
            }
        }
    }
}

bool UUltraDynamicSkyBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

TStatId UUltraDynamicSkyBridge::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UUltraDynamicSkyBridge, STATGROUP_Tickables);
}

void UUltraDynamicSkyBridge::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    // Акторы могут прийти стримингом позже и по отдельности -- искать раз в
    // секунду, пока не найдены оба, а не каждый кадр.
    if ((!SkyActor.IsValid() || !WeatherActor.IsValid()) && World->GetTimeSeconds() >= NextBindAttemptSeconds)
    {
        NextBindAttemptSeconds = World->GetTimeSeconds() + 1.0;
        BindActors();
    }

    AGridWorldManager* Manager = CachedManager.Get();
    if (!Manager)
    {
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            Manager = *It;
            CachedManager = Manager;
            break;
        }
    }
    if (!Manager)
    {
        return;
    }

    if (bWeatherPulled && !WeatherActor.IsValid())
    {
        // UDW выгружен или удалён -- симуляция возвращается к своему шуму.
        Manager->ClearWeatherBridge();
        bWeatherPulled = false;
    }

    TryApplyPendingState(*Manager);
    PushTime(*Manager);
    bWeatherPulled |= PullWeather(*Manager);
}

bool UUltraDynamicSkyBridge::BindActors()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // Классы -- один раз: битый путь в настройках иначе грузился бы и
    // предупреждал раз в секунду.
    if (!bClassesResolved)
    {
        bClassesResolved = true;
        if (const UHerbalistSettings* Settings = GetHerbalistSettings())
        {
            SkyClass = Settings->UltraDynamicSkyClass.LoadSynchronous();
            WeatherClass = Settings->UltraDynamicWeatherClass.LoadSynchronous();
        }
    }

    auto FindActorOfClass = [World](UClass* Class) -> AActor*
    {
        if (!Class) return nullptr;
        for (TActorIterator<AActor> It(World, Class); It; ++It)
        {
            return *It;
        }
        return nullptr;
    };

    AActor* Sky = SkyActor.IsValid() ? SkyActor.Get() : FindActorOfClass(SkyClass.Get());
    AActor* Weather = WeatherActor.IsValid() ? WeatherActor.Get() : FindActorOfClass(WeatherClass.Get());
    const bool bNewlyFound = (Sky && !SkyActor.IsValid()) || (Weather && !WeatherActor.IsValid());
    if (!bNewlyFound)
    {
        return Sky || Weather;
    }

    BindActorsForTest(Sky, Weather);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Sky] Мост UDS/UDW: небо %s, погода %s"),
        Sky ? *Sky->GetName() : TEXT("нет"), Weather ? *Weather->GetName() : TEXT("нет"));
    return true;
}

void UUltraDynamicSkyBridge::BindActorsForTest(AActor* InSky, AActor* InWeather)
{
    if (SkyActor.Get() != InSky)
    {
        SkyActor = InSky;
        bSkyConfigured = false;
        LastPushedDayIndex = INDEX_NONE;
    }
    if (WeatherActor.Get() != InWeather)
    {
        WeatherActor = InWeather;
        bWeatherConfigured = false;
    }
    ConfigureBoundActors();
}

void UUltraDynamicSkyBridge::ConfigureBoundActors()
{
    AActor* Sky = SkyActor.Get();
    if (Sky && !bSkyConfigured)
    {
        bSkyConfigured = true;
        // Время ведут часы симуляции: свой ход, системное и летнее время UDS
        // шли бы параллельно и спорили с мостом.
        SetUdsBool(Sky, UltraDynamicSkyNames::AnimateTimeOfDay, false);
        SetUdsBool(Sky, UltraDynamicSkyNames::UseSystemTime, false);
        if (UFunction* SetDst = FindUdsFunction(Sky, UltraDynamicSkyNames::SetApplyDaylightSavingsTime))
        {
            FUdsCallParams Params(SetDst);
            if (FBoolProperty* On = Params.FindBoolParam())
            {
                On->SetPropertyValue_InContainer(Params.Buffer.GetData(), false);
            }
            Sky->ProcessEvent(SetDst, Params.Buffer.GetData());
        }
    }

    AActor* Weather = WeatherActor.Get();
    if (Weather && !bWeatherConfigured)
    {
        bWeatherConfigured = true;
        // Сезон UDW -- от даты UDS, метеорологический, как у календаря
        // симуляции (решение пользователя, §8 плана).
        SetUdsByte(Weather, UltraDynamicSkyNames::SeasonMode, UltraDynamicSkyNames::SeasonModeUseUdsDate);
        SetUdsBool(Weather, UltraDynamicSkyNames::MeteorologicalSeasons, true);
        SetUdsInt(Weather, UltraDynamicSkyNames::SeasonDayOffset, 0);
    }
}

double UUltraDynamicSkyBridge::UdsTimeOfDay(float TimeOfDay01, float DayMinutes, double DawnTime, double DuskTime, double TwilightHalfHours)
{
    const double Half = FMath::Clamp(TwilightHalfHours, 0.0, 6.0) * 100.0;
    const double Dawn = FMath::Clamp(DawnTime, 0.0, 2400.0);
    if (DayMinutes < 18.0f)
    {
        return FMath::Fmod(Dawn - Half + 2400.0, 2400.0);
    }

    const double Day = DayMinutes;
    const double Minute = FMath::Clamp(static_cast<double>(TimeOfDay01), 0.0, 1.0) * Day;

    // Минуты симуляции (§15.2) и часы UDS в опорных точках; последняя --
    // начало следующего рассвета, на сутки UDS позже. Часы не убывают даже
    // при странных Dawn/Dusk у актора.
    const double Minutes[5] = { 0.0, 6.0, Day - 12.0, Day - 6.0, Day };
    double Hours[5] = { Dawn - Half, Dawn + Half, DuskTime - Half, DuskTime + Half, Dawn - Half + 2400.0 };
    for (int32 Index = 1; Index < 4; ++Index)
    {
        Hours[Index] = FMath::Clamp(Hours[Index], Hours[Index - 1], Hours[4]);
    }

    double Result = Hours[4];
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (Minute < Minutes[Index + 1])
        {
            const double Alpha = (Minute - Minutes[Index]) / FMath::Max(Minutes[Index + 1] - Minutes[Index], KINDA_SMALL_NUMBER);
            Result = FMath::Lerp(Hours[Index], Hours[Index + 1], Alpha);
            break;
        }
    }
    Result = FMath::Fmod(Result, 2400.0);
    return Result < 0.0 ? Result + 2400.0 : Result;
}

void UUltraDynamicSkyBridge::UdsDate(int32 DayOfYear, int32 YearIndex, int32& OutMonth, int32& OutDay, int32& OutYear)
{
    const HerbalistCore::Calendar::FCalendarDate Date = HerbalistCore::Calendar::DateFromDayOfYear(DayOfYear);
    OutMonth = Date.Month;
    OutDay = Date.Day;
    OutYear = UdsBaseYear + FMath::Max(YearIndex, 0) + (Date.Month < HerbalistCore::Calendar::FirstMonth ? 1 : 0);
}

float UUltraDynamicSkyBridge::WeatherToSimulation(double UdwValue)
{
    return FMath::Clamp(static_cast<float>(UdwValue / 10.0), 0.0f, 1.0f);
}

void UUltraDynamicSkyBridge::PushTime(const AGridWorldManager& Manager)
{
    AActor* Sky = SkyActor.Get();
    if (!Sky)
    {
        return;
    }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayMinutes = Settings ? Settings->GameDayMinutes : 32.0f;
    double DawnTime = 600.0;
    double DuskTime = 1800.0;
    GetUdsDouble(Sky, UltraDynamicSkyNames::DawnTime, DawnTime);
    GetUdsDouble(Sky, UltraDynamicSkyNames::DuskTime, DuskTime);

    SetUdsDouble(Sky, UltraDynamicSkyNames::TimeOfDay, UdsTimeOfDay(Manager.GetTimeOfDay01(), DayMinutes, DawnTime, DuskTime,
        Settings ? Settings->UltraDynamicSkyTwilightHalfHours : 1.0f));

    // Дата -- только при смене суток: UDS пересчитывает от неё солнце и сезон.
    const int32 DayIndex = Manager.GetGameDayIndex();
    if (DayIndex != LastPushedDayIndex)
    {
        LastPushedDayIndex = DayIndex;
        int32 Month = 3, Day = 1, Year = UdsBaseYear;
        UdsDate(DayIndex % HerbalistCore::Calendar::DaysPerYear, DayIndex / HerbalistCore::Calendar::DaysPerYear, Month, Day, Year);
        SetUdsInt(Sky, UltraDynamicSkyNames::Month, Month);
        SetUdsInt(Sky, UltraDynamicSkyNames::Day, Day);
        SetUdsInt(Sky, UltraDynamicSkyNames::Year, Year);
    }
}

bool UUltraDynamicSkyBridge::PullWeather(AGridWorldManager& Manager)
{
    AActor* Weather = WeatherActor.Get();
    const FObjectProperty* StateProperty = FindUdsProperty<FObjectProperty>(Weather, UltraDynamicSkyNames::GlobalWeatherState);
    const UObject* State = StateProperty ? StateProperty->GetObjectPropertyValue_InContainer(Weather) : nullptr;
    if (!State)
    {
        return false;
    }

    double Rain = 0.0, Snow = 0.0, Wind = 0.0, Fog = 0.0;
    const bool bAllRead = GetUdsDouble(State, UltraDynamicSkyNames::Rain, Rain)
        & GetUdsDouble(State, UltraDynamicSkyNames::Snow, Snow)
        & GetUdsDouble(State, UltraDynamicSkyNames::WindIntensity, Wind)
        & GetUdsDouble(State, UltraDynamicSkyNames::Fog, Fog);
    if (!bAllRead)
    {
        return false;
    }

    Manager.SetWeatherBridgeIntensities(WeatherToSimulation(Rain), WeatherToSimulation(Snow),
        WeatherToSimulation(Wind), WeatherToSimulation(Fog));
    return true;
}

FString UUltraDynamicSkyBridge::CaptureState()
{
    AActor* Sky = SkyActor.Get();
    UFunction* Create = FindUdsFunction(Sky, UltraDynamicSkyNames::CreateStateForSaving);
    if (!Create)
    {
        return FString();
    }

    FUdsCallParams Params(Create);
    FStructProperty* StateParam = Params.FindStateParam();
    if (!StateParam)
    {
        return FString();
    }
    Sky->ProcessEvent(Create, Params.Buffer.GetData());
    ClearWeatherOverrideVolumes(StateParam, Params.Buffer.GetData());

    // Экспорт относительно значений по умолчанию структуры: разбор при
    // загрузке начинает с них же (параметр функции инициализируется ими), и
    // пропущенное в тексте поле получает то же значение, что было. Без этого
    // экспорт опускал нули, а у структуры UDW по умолчанию -1 ("не задано") --
    // ясная погода превращалась бы в "не задано" (тест разбора и сборки).
    TArray<uint8> Defaults;
    Defaults.SetNumZeroed(StateParam->Struct->GetStructureSize());
    StateParam->Struct->InitializeStruct(Defaults.GetData());
    FString Text;
    StateParam->ExportTextItem_Direct(Text, StateParam->ContainerPtrToValuePtr<void>(Params.Buffer.GetData()), Defaults.GetData(), nullptr, PPF_None);
    StateParam->Struct->DestroyStruct(Defaults.GetData());
    return Text;
}

void UUltraDynamicSkyBridge::QueueStateForLoad(const FString& State, const AGridWorldManager& Manager)
{
    PendingState = State;
    if (!SkyActor.IsValid())
    {
        BindActors();
    }
    TryApplyPendingState(Manager);
}

bool UUltraDynamicSkyBridge::TryApplyPendingState(const AGridWorldManager& Manager)
{
    AActor* Sky = SkyActor.Get();
    if (PendingState.IsEmpty() || !Sky || !Sky->HasActorBegunPlay())
    {
        return false;
    }

    const FString State = MoveTemp(PendingState);
    PendingState.Reset();
    if (!ApplyState(State, Manager))
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Sky] Состояние неба и погоды из сейва не применено -- погода остаётся текущей"));
        return false;
    }
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Sky] Состояние неба и погоды из сейва применено"));
    return true;
}

bool UUltraDynamicSkyBridge::ApplyState(const FString& State, const AGridWorldManager& Manager)
{
    AActor* Sky = SkyActor.Get();
    UFunction* Apply = FindUdsFunction(Sky, UltraDynamicSkyNames::ApplySavedState);
    if (!Apply || State.IsEmpty())
    {
        return false;
    }

    {
        FUdsCallParams Params(Apply);
        FStructProperty* StateParam = Params.FindStateParam();
        if (!StateParam)
        {
            return false;
        }

        // Разбор целиком или ничего: переименованное в новой версии плагина
        // поле или неразрешённая ссылка дают только предупреждение, а
        // применять полуразобранное состояние хуже, чем оставить текущее.
        FStringOutputDevice Errors;
        const TCHAR* End = StateParam->ImportText_InContainer(*State, Params.Buffer.GetData(), nullptr, PPF_None, &Errors);
        if (!End || !Errors.IsEmpty() || !FString(End).TrimStartAndEnd().IsEmpty())
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("[Sky] Состояние UDS/UDW из сейва не разобрано: %s"),
                Errors.IsEmpty() ? TEXT("лишний текст после структуры") : *Errors);
            return false;
        }
        Sky->ProcessEvent(Apply, Params.Buffer.GetData());
    }

    // Время и дата в структуре -- на момент сейва по часам UDS; мост ставит
    // их из часов симуляции, уже восстановленных загрузкой.
    LastPushedDayIndex = INDEX_NONE;
    PushTime(Manager);
    if (UFunction* HardReset = FindUdsFunction(Sky, UltraDynamicSkyNames::HardResetCache))
    {
        Sky->ProcessEvent(HardReset, nullptr);
    }
    return true;
}
