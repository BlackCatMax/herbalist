// AmbientEntityTypes.h
//
// Низший ранг бестиария (16_Entity_Manifestation.md §16.2): "амбиентная
// зона, не объект" — условие есть переход Meta-оси клетки через порог
// (плюс, опционально, ночная фаза), эффект есть нудж TargetState теми же
// осями. §16.2 прямо говорит: "новый код не нужен — только данные (порог +
// d_manifest)". До аудита 2026-08-24 это было не так: единственный
// реализованный низший (Гнильники) жил как один бесповоротно захардкоженный
// if-блок в GridWorldManagerEntities.cpp, так что "просто данные" на деле
// означало "скопировать ещё один if-блок на каждое новое существо". Этот
// файл превращает раздел в то, чем он и задуман — таблицу определений,
// которую обходит один универсальный цикл (UpdateEntityManifestations).
//
// Раньше здесь стоял комментарий "сознательно НЕ вынесено в UDataTable" —
// 2026-09-02, прямой запрос пользователя ("чёткая дата-драйвен архитектура
// по всем карточкам всего проекта"), решение пересмотрено явно, с полным
// пониманием исходной причины: новый ТИП условия (новое значение
// EAmbientTriggerAxis, новый bRequiresX-гейт) по-прежнему требует правки
// C++ — DataTable снимает только необходимость пересборки при ТЮНИНГЕ
// значений уже существующих 28 карточек (пороги/ставки/биом/гейты), не
// изобретение нового языка условий. `GetAmbientEntityDefinitions()` ниже
// теперь лениво грузит `/Game/Herbalist/Data/DT_AmbientEntities`
// (`Core/Entities/AmbientEntitiesCreateCommandlet` создаёт ассет с нуля,
// строка в строку перенося прежний литеральный массив) — тот же паттерн,
// что уже `FBiomeDefaults` (Core/Types/BiomeTypes.cpp), но ленивый, не
// push-инициализируемый: headless-автотесты никогда не вызывают
// AProjectHerbalistGameModeBase::BeginPlay() (подтверждено эмпирически —
// BiomeDataTable у Biome тоже никогда не заполнен в тестовом мире, там
// это безобидно, тесты просто не читают дефолты; здесь так же оставить
// было нельзя — 15+ существующих тестов зовут эту функцию напрямую,
// ожидая реальные 28 карточек). Function-local static кэш строится один
// раз при первом вызове, из любого контекста — реальной игры, PIE или
// голого editor-теста — без внешней инициализации.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Engine/DataTable.h"
#include "AmbientEntityTypes.generated.h"

UENUM()
enum class EAmbientTriggerAxis : uint8
{
    None,          // условие только по ночи (bRequiresNight), без оси
    Corruption,
    Purity,
    Distortion,
    Stability,
    // Не Meta вовсе (FGridCell::HarvestStress) — добавлено 2026-08-29 для
    // Злыдни ("заброшенное жильё, накопленный HarvestStress", §16.2).
    // GetAmbientTriggerAxisValue поэтому принимает Cell целиком, не Meta.
    HarvestStress,
    // Direction-оси, не Meta — добавлены 2026-08-29 для закрытия бестиария
    // (Трясинные духи: "Nature-ось доминирует"; Чащобные духи: "Nature
    // экстремум"). Тот же принцип, что уже применён к ELandmarkAxis
    // (LandmarkTypes.h) — Direction читается тем же аппаратом, что Meta.
    Body,
    Mind,
    Spirit,
    Nature
};

// Погода (§15.7) — 2026-08-29, собственный C++-сигнал (GridWorldManager::
// GetWindIntensity/GetSnowIntensity/IsWindy/IsBlizzard), не Ultra Dynamic
// Weather (плагин пока не установлен в проект). Только два условия — ровно
// то, что нужно трём картам (Ветряные бесы/Вихри: Wind; Метельники: Blizzard).
UENUM()
enum class EWeatherCondition : uint8
{
    Wind,
    Blizzard
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FAmbientEntityDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY() FName EntityID;
    UPROPERTY() EBiomeType Biome = EBiomeType::Bog;

    // Явный порядок регистрации (2026-09-02, миграция на DataTable) --
    // задокументированные тай-брейки этого файла ("Метельники ДО Ледяных
    // духов/Шептунов", "Омутные огни ДО Русалок" и т.д.) читают порядок
    // объявления в прежнем литеральном массиве. TMap/DataTable::GetRowMap()
    // не даёт формальной гарантии стабильного порядка итерации -- этот
    // явный int делает тай-брейк независимым от деталей реализации
    // контейнера. GetAmbientEntityDefinitions() ниже сортирует по этому
    // полю перед возвратом кэша.
    UPROPERTY() int32 SortOrder = 0;

    // Оба false = клетка любая (земля или вода). Ставить оба true бессмысленно.
    UPROPERTY() bool bLandOnly = false;
    UPROPERTY() bool bWaterOnly = false;

    // Условие проявления. TriggerAxis == None означает "нет условия по оси" —
    // тогда обязан быть хотя бы один из bRequiresNight/bRequiresSeason,
    // иначе определение сработает всегда (проверяется в
    // GetAmbientEntityDefinitions() через check).
    UPROPERTY() EAmbientTriggerAxis TriggerAxis = EAmbientTriggerAxis::None;
    UPROPERTY() float TriggerThreshold = 0.0f;
    UPROPERTY() bool bTriggerAbove = true;   // true: ось > порога; false: ось < порога
    UPROPERTY() bool bRequiresNight = false;
    UPROPERTY() float HysteresisMargin = 0.05f;

    // Минимальная дистанция до другого проявления ЭТОГО ЖЕ вида, В МЕТРАХ
    // (2026-09-03, жалоба пользователя "как уменьшить плотность существ?
    // слишком много ... спавнятся буквально каждые 3 метра").
    //
    // До этого поля плотность существ вообще нечем было регулировать:
    // потолок задавала сама сетка -- одно поле Cell.ManifestedEntityID, то
    // есть максимум одна сущность на клетку, а значит одна на CellSize.
    // Порог (TriggerThreshold) решает "может ли здесь появиться", но НЕ
    // "как редко" -- поднимая его, получаешь те же плотные заросли, просто
    // на меньшей площади. Дистанция -- ортогональная ручка: где возможно
    // (порог) и насколько разреженно (это поле).
    //
    // Дистанция ПО ВИДУ, не по всем существам сразу -- прямое пожелание
    // пользователя: "Гнильники могут кишеть, а Лихо встречается раз на
    // километр". Виды не мешают друг другу занимать соседние клетки.
    //
    // В МЕТРАХ, а не в клетках -- чтобы поведение не поехало при смене
    // CellSize (ровно тем и обожглись на высотном поясе тем же днём), и не
    // в сантиметрах -- это дизайнерская величина уровня "раз на километр",
    // читаемая глазами, как уже существующий ActiveSimulationRadiusMeters.
    //
    // 0 (дефолт) -- выключено, прежнее поведение "одна на клетку".
    //
    // Дефолт НЕ ставится ненулевым намеренно, хотя соблазн был: первая
    // версия правки поставила 30 м и уронила давний честный тест
    // Herbalist.AmbientEntity.MezhevyeOnlyManifestOnBiomeBorder. Причина
    // поучительная и прямо следует из "метры, а не клетки": тестовый мир
    // живёт на CellSize=100 (клетка 1 м) и сетке 20x20, то есть 20 м
    // поперёк -- любая дистанция от ~20 м там подавляет ВСЁ, включая
    // клетку, которую тест проверяет. В боевом мире (CellSize=1000) те же
    // 30 м -- всего 3 клетки. Значение грид-независимо, как и задумано,
    // но "разумное для игры" и "разумное для тестового мира" расходятся
    // на порядок, и молча ломать существующие тесты чужим дефолтом
    // неправильно.
    //
    // Поэтому: механизм есть, включается поштучно на виде в
    // DT_AmbientEntities (кто кишит -- 0 или единицы метров, кто редок --
    // сотни). Раздавать 33 вида "по ощущению" парсер/код не должен -- это
    // геймдизайн, тот же принцип, что уже у нулевых ставок эффекта.
    UPROPERTY() float MinSpacingMeters = 0.0f;

    // Третий временной гейт, независимый от Ночи/Сезона — добавлен
    // 2026-08-29 для существ §16.2, чей триггер буквально "сумерки", не
    // "ночь" (Шишиги: "овраг/куст, сумерки"). IsDusk() уже существует и
    // используется §16.5 ("Морок просыпается раньше, чем стемнеет") —
    // здесь просто второй потребитель того же читателя фазы суток.
    UPROPERTY() bool bRequiresDusk = false;

    // Второй, независимый от ночи временной гейт — добавлен 2026-08-29 для
    // существ §16.2, чей триггер завязан на сезон (Ледяные духи: "низкая
    // температура" -> Зима; Суховейки: "засушливый сезон" -> Лето), а не на
    // время суток. Оба гейта (ночь + сезон) можно сочетать одновременно.
    UPROPERTY() bool bRequiresSeason = false;
    UPROPERTY() ESeason RequiredSeason = ESeason::Spring;

    // Четвёртый гейт — не время, а форма клетки в сетке: граничит ли она с
    // клеткой ДРУГОГО биома (Chebyshev-соседи, 4 напрямую). Добавлено
    // 2026-08-29 для Межевых ("клетка на границе биомов графа") — реальная,
    // не приближённая проверка: тот же самый приём соседей уже применён к
    // Пограничному капищу (BiomeGraphSubsystem.cpp,
    // CollectBorderShrineDamping), здесь просто второй потребитель.
    UPROPERTY() bool bRequiresBiomeBorder = false;

    // Пятый гейт — фаза луны, читает уже существующий GetMoonPhase() (§15.3).
    // Добавлено 2026-08-29 для Омутных огней ("глубокий омут, БЕЗЛУННАЯ
    // ночь" -> Новолуние + Ночь одновременно). "Глубокий омут" как атрибут
    // клетки не моделируется — тот же принцип, что уже применён ко всем
    // остальным водным существам поймы (Русалки/Кувшинкины/Водяные бесы/
    // Плескуны тоже не различают глубину, просто bWaterOnly).
    UPROPERTY() bool bRequiresMoonPhase = false;
    UPROPERTY() EMoonPhase RequiredMoonPhase = EMoonPhase::NewMoon;

    // Шестой гейт — погода (собственный C++-сигнал, см. EWeatherCondition
    // выше). Добавлено 2026-08-29 для Ветряных бесов/Метельников/Вихрей —
    // изначально заблокированных отсутствием погодной системы, теперь
    // разблокированных.
    UPROPERTY() bool bRequiresWeather = false;
    UPROPERTY() EWeatherCondition RequiredWeather = EWeatherCondition::Wind;

    // Седьмой/восьмой гейты — окна внутри сезона (GridWorldManager::
    // IsLateSummer/IsKupalaNight, §15.4/HerbalistSettings.h). Добавлены
    // 2026-08-29 для Листовиков ("осень", проект признаёт только три
    // сезона) и Купальских (нужен календарь, которого нет как отдельной
    // системы) — оба прямые решения пользователя: не заводить четвёртый
    // сезон/полноценный календарь, обойтись узкими окнами внутри Лета.
    UPROPERTY() bool bRequiresLateSummer = false;
    UPROPERTY() bool bRequiresKupalaNight = false;

    // Нудж TargetState, "в секунду" (как GnilnikiNudgeRate раньше) —
    // умножается на DeltaTime в UpdateEntityManifestations. Ноль = не трогать ось.
    UPROPERTY() float CorruptionRate = 0.0f;
    UPROPERTY() float PurityRate = 0.0f;
    UPROPERTY() float DistortionRate = 0.0f;
    UPROPERTY() float StabilityRate = 0.0f;
    // Добавлены 2026-08-29 вместе с Potency/Resonance-осями и Magnitude —
    // первые три существа (Гнильники/Моховые/Степные огни) не тронули
    // остальные три возможных цели нуджа, но §16.2 их называет
    // (Кувшинкины духи: Resonance; Ледяные духи/Суховейки: Magnitude).
    UPROPERTY() float PotencyRate = 0.0f;
    UPROPERTY() float ResonanceRate = 0.0f;
    // Magnitude — не под Meta (FRealState::Magnitude, отдельное поле),
    // применяется отдельной строкой в UpdateEntityManifestations.
    UPROPERTY() float MagnitudeRate = 0.0f;
    // Direction (не Meta) — добавлено 2026-08-29 для Межевых ("клетка на
    // границе биомов графа -> Nature↑"), первый Низший, чей ЭФФЕКТ (не
    // только триггер выше) бьёт по Direction. Только Nature заведена — не
    // нужны Body/Mind/Spirit, пока ни одно существо не просит их как эффект.
    UPROPERTY() float NatureRate = 0.0f;

    // Физическое представление (2026-08-30, "заводим родительские классы для
    // сущностей") — пусто = базовый AAmbientEntityActor (невидимый маркер).
    // Конкретный Blueprint-наследник на существо (меш/партиклы) — контент,
    // добавляется позже, не блокирует спавн/деспавн.
    UPROPERTY() TSubclassOf<class AHerbalistEntityActor> ActorClass;
};

// Обратная операция к GetAmbientTriggerAxisValue ниже -- нужна тестам,
// которым нужно программно завести клетку в состояние "подходит под этот
// Axis/Threshold", не только читать уже готовую (SystemInteractionTest.cpp,
// 2026-08-30, "проверим сочетания всех биомов").
inline void SetAmbientTriggerAxisValue(FGridCell& Cell, EAmbientTriggerAxis Axis, float Value)
{
    switch (Axis)
    {
    case EAmbientTriggerAxis::Corruption:    Cell.State.Meta.Corruption = Value; break;
    case EAmbientTriggerAxis::Purity:        Cell.State.Meta.Purity = Value; break;
    case EAmbientTriggerAxis::Distortion:    Cell.State.Meta.Distortion = Value; break;
    case EAmbientTriggerAxis::Stability:     Cell.State.Meta.Stability = Value; break;
    case EAmbientTriggerAxis::HarvestStress: Cell.HarvestStress = Value; break;
    case EAmbientTriggerAxis::Body:          Cell.State.Direction.Body = Value; break;
    case EAmbientTriggerAxis::Mind:          Cell.State.Direction.Mind = Value; break;
    case EAmbientTriggerAxis::Spirit:        Cell.State.Direction.Spirit = Value; break;
    case EAmbientTriggerAxis::Nature:        Cell.State.Direction.Nature = Value; break;
    default: break;
    }
}

inline float GetAmbientTriggerAxisValue(const FGridCell& Cell, EAmbientTriggerAxis Axis)
{
    switch (Axis)
    {
    case EAmbientTriggerAxis::Corruption:    return Cell.State.Meta.Corruption;
    case EAmbientTriggerAxis::Purity:        return Cell.State.Meta.Purity;
    case EAmbientTriggerAxis::Distortion:    return Cell.State.Meta.Distortion;
    case EAmbientTriggerAxis::Stability:     return Cell.State.Meta.Stability;
    case EAmbientTriggerAxis::HarvestStress: return Cell.HarvestStress;
    case EAmbientTriggerAxis::Body:          return Cell.State.Direction.Body;
    case EAmbientTriggerAxis::Mind:          return Cell.State.Direction.Mind;
    case EAmbientTriggerAxis::Spirit:        return Cell.State.Direction.Spirit;
    case EAmbientTriggerAxis::Nature:        return Cell.State.Direction.Nature;
    default:                                 return 0.0f;
    }
}

// Ленивая загрузка из /Game/Herbalist/Data/DT_AmbientEntities (2026-09-02,
// см. комментарий у файла выше) -- строится один раз при первом
// обращении, из любого контекста (реальная игра/PIE/голый editor-тест),
// без внешней инициализации через GameModeBase. Явный SortOrder
// гарантирует задокументированные тай-брейки регистрации независимо от
// порядка итерации DataTable::GetRowMap() (не формально гарантированного
// контейнером). Прежний check() (жёсткий крах на карточке без единого
// гейта) заменён на UE_LOG(Error) + исключение карточки -- данные теперь
// правит человек через DataTable, не ловятся компилятором, тихий баг
// конфигурации не должен ронять игру, но и не должен молча заявлять
// клетку каждый тик.
inline const TArray<FAmbientEntityDefinition>& GetAmbientEntityDefinitions()
{
    static const TArray<FAmbientEntityDefinition> Definitions = []()
    {
        check(IsInGameThread());   // LoadObject не потокобезопасен

        TArray<FAmbientEntityDefinition> Defs;
        UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_AmbientEntities"));
        if (!Table)
        {
            // LogTemp, не LogHerbalistData -- эта функция inline, компилируется
            // в ЛЮБОЙ модуль, который её вызывает (ProjectHerbalist,
            // ProjectHerbalistTests), а категории HerbalistLogChannels.h не
            // несут API-экспорта (безопасны только внутри своего модуля) --
            // словили LNK2001 на этом ровно один раз, до коммита.
            UE_LOG(LogTemp, Error, TEXT("GetAmbientEntityDefinitions: не удалось загрузить DT_AmbientEntities -- Низший ранг бестиария будет пуст"));
            return Defs;
        }
        // Тот же GC-фикс, что уже FBiomeDefaults (Core/Types/BiomeTypes.cpp) --
        // сырой static-указатель не отслеживается GC, без рутования таблица
        // собиралась бы примерно через 60 сек в headless-прогонах.
        Table->AddToRoot();

        TArray<FAmbientEntityDefinition*> Rows;
        Table->GetAllRows(TEXT("GetAmbientEntityDefinitions"), Rows);
        Defs.Reserve(Rows.Num());
        for (const FAmbientEntityDefinition* Row : Rows)
        {
            if (!Row) continue;

            const bool bHasGate = Row->TriggerAxis != EAmbientTriggerAxis::None || Row->bRequiresNight || Row->bRequiresSeason
                || Row->bRequiresDusk || Row->bRequiresBiomeBorder || Row->bRequiresMoonPhase || Row->bRequiresWeather
                || Row->bRequiresLateSummer || Row->bRequiresKupalaNight;
            if (!bHasGate)
            {
                UE_LOG(LogTemp, Error, TEXT("GetAmbientEntityDefinitions: карточка '%s' без единого гейта (TriggerAxis=None и все bRequiresX=false) -- заявляла бы клетку безусловно каждый тик, исключена из реестра"),
                    *Row->EntityID.ToString());
                continue;
            }
            Defs.Add(*Row);
        }
        Defs.Sort([](const FAmbientEntityDefinition& A, const FAmbientEntityDefinition& B) { return A.SortOrder < B.SortOrder; });
        return Defs;
    }();
    return Definitions;
}
