// Source/ProjectHerbalistTests/Private/Tests/UltraDynamicSkyBridgeTest.cpp
//
// Мост Ultra Dynamic Sky / Weather (2026-09-16, этап 2 docs/research/
// DESIGN_Living_Vegetation_Research.md §5): имена, по которым мост ходит в
// Blueprint'ы плагина через отражение, опорные точки времени суток и даты, и
// живой прогон на акторах UDS/UDW -- время и дата уходят в небо, погода
// приходит в симуляцию, состояние переживает сохранение.

#include "Core/World/Sky/UltraDynamicSkyBridge.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Types/HerbalistCalendar.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    const TCHAR* SkyBridgeWeatherSettingsPath = TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/System/UDS_Weather_Settings.UDS_Weather_Settings_C");

    double ReadSkyDouble(const UObject* Object, const TCHAR* Name)
    {
        const FDoubleProperty* Property = Object ? FindFProperty<FDoubleProperty>(Object->GetClass(), Name) : nullptr;
        return Property ? Property->GetPropertyValue_InContainer(Object) : -1.0;
    }

    int32 ReadSkyInt(const UObject* Object, const TCHAR* Name)
    {
        const FIntProperty* Property = Object ? FindFProperty<FIntProperty>(Object->GetClass(), Name) : nullptr;
        return Property ? Property->GetPropertyValue_InContainer(Object) : -1;
    }

    bool ReadSkyBool(const UObject* Object, const TCHAR* Name)
    {
        const FBoolProperty* Property = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
        return Property && Property->GetPropertyValue_InContainer(Object);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSky_BridgeNamesExistInUltraDynamicSky,
    "Herbalist.Sky.BridgeNamesExistInUltraDynamicSky",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSky_BridgeNamesExistInUltraDynamicSky::RunTest(const FString& Parameters)
{
    // Обновление плагина, переименовавшее свойство или функцию, должно
    // ронять этот тест, а не молча выключать мост в игре.
    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    UClass* SkyClass = Settings->UltraDynamicSkyClass.LoadSynchronous();
    UClass* WeatherClass = Settings->UltraDynamicWeatherClass.LoadSynchronous();
    UClass* StateClass = LoadObject<UClass>(nullptr, SkyBridgeWeatherSettingsPath);
    if (!TestNotNull(TEXT("UltraDynamicSkyClass в Herbalist Settings"), SkyClass)
        || !TestNotNull(TEXT("UltraDynamicWeatherClass в Herbalist Settings"), WeatherClass)
        || !TestNotNull(TEXT("UDS_Weather_Settings"), StateClass))
    {
        return false;
    }

    auto ExpectProperty = [this](UClass* Class, const TCHAR* Name, FFieldClass* Type)
    {
        const FProperty* Property = FindFProperty<FProperty>(Class, Name);
        TestTrue(*FString::Printf(TEXT("%s.%s есть и типа %s"), *Class->GetName(), Name, *Type->GetName()),
            Property && Property->IsA(Type));
    };
    ExpectProperty(SkyClass, TEXT("Time of Day"), FDoubleProperty::StaticClass());
    ExpectProperty(SkyClass, TEXT("Animate Time of Day"), FBoolProperty::StaticClass());
    ExpectProperty(SkyClass, TEXT("Use System Time"), FBoolProperty::StaticClass());
    ExpectProperty(SkyClass, TEXT("Month"), FIntProperty::StaticClass());
    ExpectProperty(SkyClass, TEXT("Day"), FIntProperty::StaticClass());
    ExpectProperty(SkyClass, TEXT("Year"), FIntProperty::StaticClass());
    ExpectProperty(SkyClass, TEXT("Dawn Time"), FDoubleProperty::StaticClass());
    ExpectProperty(SkyClass, TEXT("Dusk Time"), FDoubleProperty::StaticClass());
    ExpectProperty(WeatherClass, TEXT("Season Mode"), FByteProperty::StaticClass());
    ExpectProperty(WeatherClass, TEXT("Meteorological Seasons"), FBoolProperty::StaticClass());
    ExpectProperty(WeatherClass, TEXT("Season Day Offset"), FIntProperty::StaticClass());
    ExpectProperty(WeatherClass, TEXT("Global Weather State"), FObjectProperty::StaticClass());
    for (const TCHAR* Name : { TEXT("Rain"), TEXT("Snow"), TEXT("Wind Intensity"), TEXT("Fog") })
    {
        ExpectProperty(StateClass, Name, FDoubleProperty::StaticClass());
    }

    for (const TCHAR* Name : { TEXT("Set Apply Daylight Savings Time"), TEXT("Create UDS and UDW State for Saving"),
                               TEXT("Apply Saved UDS and UDW State"), TEXT("Hard Reset Cache") })
    {
        TestNotNull(*FString::Printf(TEXT("Функция UDS «%s»"), Name), SkyClass->FindFunctionByName(Name));
    }

    // Режим сезона 0 -- «от даты UDS»: мост пишет число, а не имя.
    if (UEnum* SeasonMode = LoadObject<UEnum>(nullptr, TEXT("/Game/UltraDynamicSky/Blueprints/Enum/UDS_SeasonMode.UDS_SeasonMode")))
    {
        TestTrue(TEXT("UDS_SeasonMode 0 -- Use UDS Date"),
            SeasonMode->GetDisplayNameTextByValue(0).ToString().Contains(TEXT("UDS Date")));
    }
    else
    {
        AddError(TEXT("UDS_SeasonMode не загружается"));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSky_TimeOfDayMapsPhasesOntoUdsDawnAndDusk,
    "Herbalist.Sky.TimeOfDayMapsPhasesOntoUdsDawnAndDusk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSky_TimeOfDayMapsPhasesOntoUdsDawnAndDusk::RunTest(const FString& Parameters)
{
    const float Day = 32.0f;
    auto At = [Day](float Minute) { return UUltraDynamicSkyBridge::UdsTimeOfDay(Minute / Day, Day, 600.0, 1800.0, 1.0); };

    // Опорные точки §15.2: середина рассвета -- Dawn Time, заката -- Dusk Time.
    TestTrue(TEXT("Начало рассвета -- 05:00"), FMath::IsNearlyEqual(At(0.0f), 500.0, 0.01));
    TestTrue(TEXT("Середина рассвета -- Dawn Time"), FMath::IsNearlyEqual(At(3.0f), 600.0, 0.01));
    TestTrue(TEXT("Начало дня -- 07:00"), FMath::IsNearlyEqual(At(6.0f), 700.0, 0.01));
    TestTrue(TEXT("Середина заката -- Dusk Time"), FMath::IsNearlyEqual(At(23.0f), 1800.0, 0.01));
    TestTrue(TEXT("Начало ночи -- 19:00"), FMath::IsNearlyEqual(At(26.0f), 1900.0, 0.01));
    TestTrue(TEXT("Полночь -- внутри ночи"), FMath::IsNearlyEqual(At(26.0f + 6.0f * 5.0f / 10.0f), 0.0, 0.01)
        || FMath::IsNearlyEqual(At(26.0f + 6.0f * 5.0f / 10.0f), 2400.0, 0.01));

    // Сутки посекундно: часы UDS идут вперёд без скачков, кроме перехода 2400 -> 0.
    double Previous = At(0.0f);
    double WorstStep = 0.0;
    for (int32 Second = 1; Second <= 32 * 60; ++Second)
    {
        const double Current = At(FMath::Fmod(Second / 60.0f, Day));
        double Step = Current - Previous;
        if (Step < -1200.0) Step += 2400.0;
        TestTrue(TEXT("Часы UDS не идут назад"), Step >= -0.001);
        WorstStep = FMath::Max(WorstStep, Step);
        Previous = Current;
    }
    // Самый быстрый участок -- ночь: 1000 часовых единиц за 360 секунд.
    TestTrue(*FString::Printf(TEXT("Без скачков (худший шаг %.3f)"), WorstStep), WorstStep < 3.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSky_CalendarDateMapsOntoUdsDate,
    "Herbalist.Sky.CalendarDateMapsOntoUdsDate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSky_CalendarDateMapsOntoUdsDate::RunTest(const FString& Parameters)
{
    struct FCase { int32 DayOfYear; int32 YearIndex; int32 Month; int32 Day; int32 Year; };
    for (const FCase& Case : { FCase{ 0, 0, 3, 1, 2001 }, FCase{ 114, 0, 6, 23, 2001 }, FCase{ 305, 0, 12, 31, 2001 },
                               FCase{ 306, 0, 1, 1, 2002 }, FCase{ 364, 0, 2, 28, 2002 }, FCase{ 0, 1, 3, 1, 2002 } })
    {
        int32 Month = 0, Day = 0, Year = 0;
        UUltraDynamicSkyBridge::UdsDate(Case.DayOfYear, Case.YearIndex, Month, Day, Year);
        TestTrue(*FString::Printf(TEXT("День %d года %d -> %02d.%02d.%d (есть %02d.%02d.%d)"),
            Case.DayOfYear, Case.YearIndex, Case.Day, Case.Month, Case.Year, Day, Month, Year),
            Month == Case.Month && Day == Case.Day && Year == Case.Year);
    }

    TestEqual(TEXT("Погода UDW 10 -> 1"), UUltraDynamicSkyBridge::WeatherToSimulation(10.0), 1.0f);
    TestEqual(TEXT("Погода UDW 2.5 -> 0.25"), UUltraDynamicSkyBridge::WeatherToSimulation(2.5), 0.25f);
    TestEqual(TEXT("Погода UDW за шкалой -> 1"), UUltraDynamicSkyBridge::WeatherToSimulation(14.0), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSky_BridgeDrivesSkyAndReadsWeather,
    "Herbalist.Sky.BridgeDrivesSkyAndReadsWeather",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSky_BridgeDrivesSkyAndReadsWeather::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    UUltraDynamicSkyBridge* Bridge = World->GetSubsystem<UUltraDynamicSkyBridge>();

    // В редакторном мире движок не исполняет Blueprint-функции акторов (не
    // CallInEditor) -- без этого сейв UDS молча ничего бы не делал, и тест
    // проверял бы пустую структуру (ревью этапа 2).
    TGuardValue<bool> AllowScript(GAllowActorScriptExecutionInEditor, true);
    if (!TestNotNull(TEXT("Мост -- подсистема мира"), Bridge)) return false;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    FActorSpawnParameters SpawnParams;
    SpawnParams.ObjectFlags |= RF_Transient;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Sky = World->SpawnActor<AActor>(Settings->UltraDynamicSkyClass.LoadSynchronous(), FTransform(FVector(0, 0, -500000.0)), SpawnParams);
    AActor* Weather = World->SpawnActor<AActor>(Settings->UltraDynamicWeatherClass.LoadSynchronous(), FTransform(FVector(0, 0, -500000.0)), SpawnParams);
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    auto Cleanup = [&]()
    {
        Bridge->QueueStateForLoad(FString(), *Manager);
        Bridge->BindActorsForTest(nullptr, nullptr);
        if (Sky) Sky->Destroy();
        if (Weather) Weather->Destroy();
        if (Manager) Manager->Destroy();
    };
    if (!TestNotNull(TEXT("UDS заспавнен"), Sky) || !TestNotNull(TEXT("UDW заспавнен"), Weather) || !TestNotNull(TEXT("Менеджер"), Manager))
    {
        Cleanup();
        return false;
    }

    // Как в игре: UDS и UDW начинают игру. На L_TestDev, открытой в
    // редакторе тестов, стоят свои UDS/UDW -- заспавненная пара в BeginPlay
    // нашла бы их, поэтому ссылки друг на друга ставятся явно.
    Sky->DispatchBeginPlay();
    Weather->DispatchBeginPlay();
    auto LinkReference = [](AActor* From, const TCHAR* Name, AActor* To)
    {
        if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(From->GetClass(), Name))
        {
            Property->SetObjectPropertyValue_InContainer(From, To);
        }
    };
    LinkReference(Sky, TEXT("Ultra Dynamic Weather"), Weather);
    LinkReference(Weather, TEXT("UltraDynamicSky"), Sky);

    // Привязка настраивает акторы под мост.
    Bridge->BindActorsForTest(Sky, Weather);
    TestFalse(TEXT("UDS: свой ход времени выключен"), ReadSkyBool(Sky, TEXT("Animate Time of Day")));
    TestFalse(TEXT("UDS: системное время выключено"), ReadSkyBool(Sky, TEXT("Use System Time")));
    TestTrue(TEXT("UDW: метеорологические сезоны"), ReadSkyBool(Weather, TEXT("Meteorological Seasons")));
    const FByteProperty* SeasonMode = FindFProperty<FByteProperty>(Weather->GetClass(), TEXT("Season Mode"));
    TestTrue(TEXT("UDW: сезон от даты UDS"), SeasonMode && SeasonMode->GetPropertyValue_InContainer(Weather) == 0);

    // 23 июня, середина заката -> Dusk Time UDS, дата 23.06.2001.
    const double DaySeconds = Settings->GameDayMinutes * 60.0;
    Manager->SetGameClockSeconds(HerbalistCore::Calendar::DayOfYearFromDate(6, 23) * DaySeconds + (Settings->GameDayMinutes - 9.0) * 60.0);
    Bridge->PushTime(*Manager);
    const double DuskTime = ReadSkyDouble(Sky, TEXT("Dusk Time"));
    TestTrue(*FString::Printf(TEXT("Время UDS -- Dusk Time %.0f (есть %.1f)"), DuskTime, ReadSkyDouble(Sky, TEXT("Time of Day"))),
        FMath::IsNearlyEqual(ReadSkyDouble(Sky, TEXT("Time of Day")), DuskTime, 0.5));
    TestEqual(TEXT("Месяц UDS"), ReadSkyInt(Sky, TEXT("Month")), 6);
    TestEqual(TEXT("Число UDS"), ReadSkyInt(Sky, TEXT("Day")), 23);
    TestEqual(TEXT("Год UDS"), ReadSkyInt(Sky, TEXT("Year")), UUltraDynamicSkyBridge::UdsBaseYear);

    // Погода UDW -> симуляция.
    const FObjectProperty* StateProperty = FindFProperty<FObjectProperty>(Weather->GetClass(), TEXT("Global Weather State"));
    UObject* State = StateProperty ? StateProperty->GetObjectPropertyValue_InContainer(Weather) : nullptr;
    if (TestNotNull(TEXT("У UDW есть Global Weather State"), State))
    {
        auto WriteState = [State](const TCHAR* Name, double Value)
        {
            if (FDoubleProperty* Property = FindFProperty<FDoubleProperty>(State->GetClass(), Name))
            {
                Property->SetPropertyValue_InContainer(State, Value);
            }
        };
        WriteState(TEXT("Rain"), 3.0);
        WriteState(TEXT("Snow"), 0.0);
        WriteState(TEXT("Wind Intensity"), 8.0);
        WriteState(TEXT("Fog"), 1.0);
        TestTrue(TEXT("Погода прочитана"), Bridge->PullWeather(*Manager));
        TestTrue(TEXT("Мост погоды активен"), Manager->bWeatherBridgeActive);
        TestTrue(TEXT("Дождь 3/10"), FMath::IsNearlyEqual(Manager->GetRainIntensity(), 0.3f, 1.0e-4f));
        TestTrue(TEXT("Ветер 8/10"), FMath::IsNearlyEqual(Manager->GetWindIntensity(), 0.8f, 1.0e-4f));
        TestEqual(TEXT("Снега нет"), Manager->GetSnowIntensity(), 0.0f);
    }

    // Сейв: снятое состояние несёт настоящую погоду (дождь 3 из шага выше);
    // применяется обратно, время после применения -- снова из часов симуляции.
    const FString Captured = Bridge->CaptureState();
    // Срез поля GlobalWeatherState до его закрывающей скобки.
    FString GlobalText;
    const int32 GlobalStart = Captured.Find(TEXT("GlobalWeatherState"));
    if (GlobalStart >= 0)
    {
        int32 Depth = 0;
        for (int32 Index = GlobalStart; Index < Captured.Len(); ++Index)
        {
            Depth += Captured[Index] == TEXT('(') ? 1 : Captured[Index] == TEXT(')') ? -1 : 0;
            if (Depth == 0 && Captured[Index] == TEXT(')'))
            {
                GlobalText = Captured.Mid(GlobalStart, Index - GlobalStart + 1);
                break;
            }
        }
    }
    TestTrue(*FString::Printf(TEXT("Снятое состояние несёт дождь 3 (%s)"), *GlobalText),
        GlobalText.Contains(TEXT("Rain_")) && GlobalText.Contains(TEXT("=3.000000")));
    TestFalse(TEXT("Объёмы погоды не сохраняются"), Captured.Contains(TEXT("PersistentLevel")));
    Manager->SetGameClockSeconds(HerbalistCore::Calendar::DayOfYearFromDate(1, 10) * DaySeconds + 3.0 * 60.0);
    TestTrue(TEXT("Состояние применено"), Bridge->ApplyState(Captured, *Manager));
    // Сама погода из применённого состояния доходит до объекта текущей
    // погоды на тике UDW -- в редакторном мире тика нет, это проверка PIE
    // (docs/verification/pie/04_World_State.md). Здесь -- то, за что отвечает
    // мост: текст структуры разбирается и собирается обратно без потерь.
    {
        UFunction* Create = Sky->FindFunction(TEXT("Create UDS and UDW State for Saving"));
        FStructProperty* StateParam = nullptr;
        for (TFieldIterator<FProperty> It(Create); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            StateParam = StateParam ? StateParam : CastField<FStructProperty>(*It);
        }
        if (TestNotNull(TEXT("Параметр-структура сейва UDS"), StateParam))
        {
            TArray<uint8> Value;
            Value.SetNumZeroed(StateParam->Struct->GetStructureSize());
            StateParam->Struct->InitializeStruct(Value.GetData());
            TestNotNull(TEXT("Текст состояния разбирается"), StateParam->ImportText_Direct(*Captured, Value.GetData(), nullptr, PPF_None));
            TArray<uint8> Defaults;
            Defaults.SetNumZeroed(StateParam->Struct->GetStructureSize());
            StateParam->Struct->InitializeStruct(Defaults.GetData());
            FString Again;
            StateParam->ExportTextItem_Direct(Again, Value.GetData(), Defaults.GetData(), nullptr, PPF_None);
            StateParam->Struct->DestroyStruct(Defaults.GetData());
            StateParam->Struct->DestroyStruct(Value.GetData());
            TestEqual(TEXT("Текст состояния после разбора и сборки тот же"), Again, Captured);
        }
    }
    TestFalse(TEXT("Испорченный текст не применяется"), Bridge->ApplyState(TEXT("(Garbage=1"), *Manager));

    // Загрузка до BeginPlay UDS -- состояние ждёт, а не теряется.
    Bridge->QueueStateForLoad(Captured, *Manager);
    TestTrue(TEXT("UDS ещё не начал игру -- состояние отложено"), Sky->HasActorBegunPlay() || Bridge->HasPendingState());
    TestTrue(TEXT("После применения время -- из часов (середина рассвета)"),
        FMath::IsNearlyEqual(ReadSkyDouble(Sky, TEXT("Time of Day")), ReadSkyDouble(Sky, TEXT("Dawn Time")), 0.5));
    TestEqual(TEXT("После применения месяц -- январь"), ReadSkyInt(Sky, TEXT("Month")), 1);
    TestEqual(TEXT("После применения год -- 2002"), ReadSkyInt(Sky, TEXT("Year")), UUltraDynamicSkyBridge::UdsBaseYear + 1);

    Cleanup();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
