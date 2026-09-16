// Core/World/Sky/UltraDynamicSkyBridge.h
//
// Мост Ultra Dynamic Sky / Weather (2026-09-16, этап 2 docs/research/
// DESIGN_Living_Vegetation_Research.md §5). UDS/UDW -- чистый Blueprint без
// C++ API, поэтому мост говорит с их акторами через отражение по именам
// свойств и функций (имена сверяет тест Herbalist.Sky.BridgeNamesExistInUltraDynamicSky:
// обновление плагина, переименовавшее их, упадёт в тесте, а не молча в игре).
//
// Один источник времени -- часы симуляции:
//   * время суток и дата -> UDS (свой ход времени UDS выключен);
//   * UDW выводит сезон из даты UDS (метеорологические сезоны) и сам ведёт
//     погоду (решение пользователя);
//   * глобальная погода UDW (дождь, снег, ветер, туман) -> AGridWorldManager::
//     SetWeatherBridgeIntensities -- по ней работают бестиарий и сбор.
//     Локальные зоны погоды UDW симуляция не видит: клетки мира -- не точка
//     камеры;
//   * состояние неба и погоды -> сейв (UHerbalistSaveGame::SkyAndWeatherState).
// Уровень без UDS/UDW -- мост молчит, симуляция берёт прежний шум погоды.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UltraDynamicSkyBridge.generated.h"

class AGridWorldManager;

UCLASS()
class PROJECTHERBALIST_API UUltraDynamicSkyBridge : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // Editor -- ради автотестов; тикает мост только в игровых мирах.
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    // Находит акторы UDS и UDW в мире (классы -- Herbalist Settings, дочерние
    // Blueprint'ы подходят) и настраивает их под мост: у UDS выключены свой
    // ход времени, системное и летнее время; у UDW сезон -- от даты UDS,
    // метеорологический, без сдвига. Уже найденный актор не ищется заново.
    // true -- найден хотя бы один.
    bool BindActors();
    // Тестовый шов: привязать конкретные акторы.
    void BindActorsForTest(AActor* InSky, AActor* InWeather);

    AActor* GetSkyActor() const { return SkyActor.Get(); }
    AActor* GetWeatherActor() const { return WeatherActor.Get(); }

    // Время суток и дата из часов симуляции -> UDS.
    void PushTime(const AGridWorldManager& Manager);
    // Погода UDW -> симуляция. false -- UDW нет или его состояние не читается.
    bool PullWeather(AGridWorldManager& Manager);

    // Сейв: FUDS_and_UDW_State как текст (ExportText) -- структура
    // Blueprint'а, в C++ её типа нет. Ссылки на объёмы погоды (WOV) не
    // сохраняются: это акторы уровня, их пути в PIE и в игре разные. Пусто --
    // UDS нет или он не ответил.
    FString CaptureState();

    // Загрузка: состояние ждёт, пока UDS найден и начал игру (стриминг,
    // BeginPlay UDS со своей стартовой погодой затёр бы применённое), и
    // применяется в Tick; если готов -- сразу.
    void QueueStateForLoad(const FString& State, const AGridWorldManager& Manager);
    bool HasPendingState() const { return !PendingState.IsEmpty(); }

    // Применяет сохранённое, затем время из часов симуляции (в структуре оно
    // своё) и Hard Reset Cache -- без него UDS доводит свойства 1–2 с. false --
    // UDS нет или текст не разобран целиком (тогда состояние не применяется).
    bool ApplyState(const FString& State, const AGridWorldManager& Manager);

    // ---- Чистые функции (тесты) ----

    // Доля суток симуляции -> Time of Day UDS (0..2400). Опорные точки:
    // рассвет -- Dawn Time UDS ± полуширина сумерек, закат -- Dusk Time ±
    // полуширина, между ними линейно; ночь (6 минут) растягивается на часы
    // от конца заката до начала следующего рассвета. Середина фазы рассвета
    // симуляции -- ровно Dawn Time UDS, заката -- Dusk Time. Опорные часы не
    // убывают при любых Dawn/Dusk: часы UDS не идут назад. Сутки короче 18
    // минут (фазы §15.2 не помещаются) дают начало рассвета.
    static double UdsTimeOfDay(float TimeOfDay01, float DayMinutes, double DawnTime, double DuskTime, double TwilightHalfHours);

    // День года календаря (от 1 марта) и номер года -> дата UDS. Год
    // календаря начинается 1 марта, поэтому январь и февраль -- уже
    // следующий год UDS. База -- невисокосный 2001 (у календаря симуляции
    // високосных нет). Дата меняется с началом суток симуляции -- на часах
    // UDS это начало рассвета, не полночь.
    static void UdsDate(int32 DayOfYear, int32 YearIndex, int32& OutMonth, int32& OutDay, int32& OutYear);

    static constexpr int32 UdsBaseYear = 2001;

    // Шкала погоды UDW -- 0..10, симуляции -- 0..1.
    static float WeatherToSimulation(double UdwValue);

private:
    TWeakObjectPtr<AActor> SkyActor;
    TWeakObjectPtr<AActor> WeatherActor;
    TWeakObjectPtr<AGridWorldManager> CachedManager;
    TWeakObjectPtr<UClass> SkyClass;
    TWeakObjectPtr<UClass> WeatherClass;
    bool bClassesResolved = false;
    bool bSkyConfigured = false;
    bool bWeatherConfigured = false;
    // Погода однажды пришла от UDW: когда актор пропадёт, симуляция
    // возвращается к своему шуму, а не держит последнюю погоду навсегда.
    bool bWeatherPulled = false;
    double NextBindAttemptSeconds = 0.0;
    int32 LastPushedDayIndex = INDEX_NONE;
    FString PendingState;

    void ConfigureBoundActors();
    bool TryApplyPendingState(const AGridWorldManager& Manager);
};
