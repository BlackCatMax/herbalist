// LandmarkTypes.h
//
// Основной ранг бестиария (16_Entity_Manifestation.md §16.3): "хозяин"
// конкретной клетки-обиталища, реагирующий на Respect (подношение через
// Apply-на-клетку, см. RunSimulationStep в GridWorldManagerTick.cpp — не
// здесь, формула и её обоснование там). Этот файл — только "что менять",
// не "когда менять": высокий Respect благословляет клетку по одной-двум
// осям существа, низкий — проклинает по одной. До 2026-08-29 единственный
// реализованный "хозяин" (Полевик) жил как захардкоженные Potency/Purity/
// Stability прямо в GridWorldManagerEntities.cpp — тот же долг, что уже
// был у Низшего до AmbientEntityTypes.h, тем же лечением: таблица данных,
// один обобщённый применитель.
//
// Оси заведены по данным из `04_Compendium/Бестиарий/<Существо>.md` —
// каждая карточка уже называет конкретный эффект в разделе "Алхимическое
// значение" (не всегда дословно совпадает с осью §16.3-примеров в самом
// GDD — "Полевик: Potency+Purity", "Дух Медведя: Body" — оба свежая
// проверка подтвердила как есть). Ось curse — интерпретация фольклора в
// уже существующий словарь (Corruption/Distortion/Stability), не новая
// придуманная "усталость"/"страх" и т.п., тем же принципом, что уже
// применён к Полевику ("наведение усталости" -> Stability, не новая ось).
//
// 2026-09-02, юнит 2/3 миграции бестиария на DataTable (прямой запрос
// пользователя, "чёткая дата-драйвен архитектура по всем карточкам всего
// проекта") -- тот же паттерн, что уже AmbientEntityTypes.h (юнит 1/3):
// GetLandmarkDefinitions() ниже лениво грузит /Game/Herbalist/Data/DT_Landmarks
// вместо литерального массива. См. подробное обоснование ленивой
// (не push-через-GameModeBase) загрузки в AmbientEntityTypes.h -- то же
// рассуждение целиком применимо здесь: headless-автотесты не вызывают
// AProjectHerbalistGameModeBase::BeginPlay(), несколько тестов
// (LandmarkTest.cpp) зовут GetLandmarkDefinitions()/FindLandmarkDefinition()
// напрямую, ожидая реальные 15 карточек.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Engine/DataTable.h"
#include "LandmarkTypes.generated.h"

UENUM()
enum class ELandmarkAxis : uint8
{
    None,
    // Direction (FRealState::Direction) — доля от суммы, не абсолютное
    // значение; см. комментарий у ApplyLandmarkAxisNudge ниже про NormalizeSum.
    Body,
    Mind,
    Spirit,
    Nature,
    // Meta (FRealState::Meta)
    Corruption,
    Purity,
    Distortion,
    Stability,
    Potency,
    Resonance
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FLandmarkDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY() FName EntityID;
    UPROPERTY() EBiomeType Biome = EBiomeType::ForestSteppe;

    // Явный порядок регистрации (2026-09-02, тот же приём, что уже
    // FAmbientEntityDefinition::SortOrder) -- у Landmark сегодня нет
    // задокументированной зависимости от порядка (SeedTestLandmarks ищет
    // первую свободную клетку своего биома, не соревнуется за неё с
    // другими Def), но поле заведено для единообразия и на случай будущей
    // такой зависимости.
    UPROPERTY() int32 SortOrder = 0;

    // До двух осей на благословение (Полевик и Переплут — единственные,
    // где карточка явно называет два эффекта сразу; у остальных Axis2 = None).
    UPROPERTY() ELandmarkAxis BlessAxis = ELandmarkAxis::None;
    UPROPERTY() float BlessRate = 0.0f;
    UPROPERTY() ELandmarkAxis BlessAxis2 = ELandmarkAxis::None;
    UPROPERTY() float BlessRate2 = 0.0f;

    UPROPERTY() ELandmarkAxis CurseAxis = ELandmarkAxis::None;
    UPROPERTY() float CurseRate = 0.0f;

    // Отягощённое проклятие (DESIGN_Community_And_Homestead.md §2.1,
    // 2026-08-31) — общий механизм эскалации, не специфичный для одного
    // хозяина: если AggravatedCurseThreshold > -1 (задан), при Respect НИЖЕ
    // этого порога curse усиливается вторым, более резким ударом поверх
    // обычного — не отдельная сущность, второй порог той же проекции (тот
    // же принцип, что уже даёт домашней Кикиморе появиться как эскалация
    // плохих отношений с Домовым, не независимый триггер).
    UPROPERTY() ELandmarkAxis AggravatedCurseAxis = ELandmarkAxis::None;
    UPROPERTY() float AggravatedCurseRate = 0.0f;
    UPROPERTY() float AggravatedCurseThreshold = -1.0f;

    // Физическое представление (2026-08-30) — пусто = базовый
    // ALandmarkEntityActor. См. комментарий у одноимённого поля в
    // AmbientEntityTypes.h.
    UPROPERTY() TSubclassOf<class AHerbalistEntityActor> ActorClass;

    // Домовой (2026-08-31) — регистрируется напрямую AAlchemyTableActor::
    // BeginPlay на клетке жилища, не через биом-сопоставление ниже. true
    // значит "SeedTestLandmarks должен пропустить эту запись целиком" —
    // иначе она бы ещё и посеялась в случайной клетке подходящего биома,
    // задвоив хозяина.
    UPROPERTY() bool bManualRegistrationOnly = false;
};

// Direction нужен отдельно от Meta, т.к. это разные подобъекты FRealState —
// применяется как обычный аддитивный нудж, БЕЗ немедленной перенормировки
// NewTarget.Direction самого по себе: тот же принцип, что уже используют
// RegenerateCellParameters для релаксации State к TargetState — S.Direction
// пересчитывается через NormalizeSum() при движении К цели, а не сама цель;
// TargetState.Direction не обязан всегда суммироваться в 1.0 как инвариант,
// только служить направлением, к которому тянут (см. GridWorldManagerCore.cpp,
// секция "3. Направление").
inline void ApplyLandmarkAxisNudge(FRealState& Target, ELandmarkAxis Axis, float Delta)
{
    switch (Axis)
    {
    case ELandmarkAxis::Body:       Target.Direction.Body   = FMath::Max(0.0f, Target.Direction.Body   + Delta); break;
    case ELandmarkAxis::Mind:       Target.Direction.Mind   = FMath::Max(0.0f, Target.Direction.Mind   + Delta); break;
    case ELandmarkAxis::Spirit:     Target.Direction.Spirit = FMath::Max(0.0f, Target.Direction.Spirit + Delta); break;
    case ELandmarkAxis::Nature:     Target.Direction.Nature = FMath::Max(0.0f, Target.Direction.Nature + Delta); break;
    case ELandmarkAxis::Corruption: Target.Meta.Corruption  = FMath::Clamp(Target.Meta.Corruption + Delta, 0.0f, 1.0f); break;
    case ELandmarkAxis::Purity:     Target.Meta.Purity      = FMath::Clamp(Target.Meta.Purity     + Delta, 0.0f, 1.0f); break;
    case ELandmarkAxis::Distortion: Target.Meta.Distortion  = FMath::Clamp(Target.Meta.Distortion + Delta, 0.0f, 1.0f); break;
    case ELandmarkAxis::Stability:  Target.Meta.Stability   = FMath::Clamp(Target.Meta.Stability  + Delta, 0.0f, 1.0f); break;
    case ELandmarkAxis::Potency:    Target.Meta.Potency     = FMath::Clamp(Target.Meta.Potency    + Delta, 0.0f, 1.0f); break;
    case ELandmarkAxis::Resonance:  Target.Meta.Resonance   = FMath::Clamp(Target.Meta.Resonance  + Delta, 0.0f, 1.0f); break;
    default: break;
    }
}

// 2026-09-02 (унификация Берегини, LegendaryEntityTypes.h::bFloorEffect) --
// та же ось-диспетчеризация, что ApplyLandmarkAxisNudge выше, но не
// добавляет Delta/сек, а гарантирует, что ось не ниже FloorValue -- пока
// условие карточки выполнено, мгновенное свойство, не скорость. Берегиня:
// floor Purity на 0.9 (была захардкожена в GridWorldManagerEntities.cpp).
inline void ApplyLandmarkAxisFloor(FRealState& Target, ELandmarkAxis Axis, float FloorValue)
{
    switch (Axis)
    {
    case ELandmarkAxis::Body:       Target.Direction.Body   = FMath::Max(Target.Direction.Body,   FloorValue); break;
    case ELandmarkAxis::Mind:       Target.Direction.Mind   = FMath::Max(Target.Direction.Mind,   FloorValue); break;
    case ELandmarkAxis::Spirit:     Target.Direction.Spirit = FMath::Max(Target.Direction.Spirit, FloorValue); break;
    case ELandmarkAxis::Nature:     Target.Direction.Nature = FMath::Max(Target.Direction.Nature, FloorValue); break;
    case ELandmarkAxis::Corruption: Target.Meta.Corruption  = FMath::Clamp(FMath::Max(Target.Meta.Corruption, FloorValue), 0.0f, 1.0f); break;
    case ELandmarkAxis::Purity:     Target.Meta.Purity      = FMath::Clamp(FMath::Max(Target.Meta.Purity,     FloorValue), 0.0f, 1.0f); break;
    case ELandmarkAxis::Distortion: Target.Meta.Distortion  = FMath::Clamp(FMath::Max(Target.Meta.Distortion, FloorValue), 0.0f, 1.0f); break;
    case ELandmarkAxis::Stability:  Target.Meta.Stability   = FMath::Clamp(FMath::Max(Target.Meta.Stability,  FloorValue), 0.0f, 1.0f); break;
    case ELandmarkAxis::Potency:    Target.Meta.Potency     = FMath::Clamp(FMath::Max(Target.Meta.Potency,    FloorValue), 0.0f, 1.0f); break;
    case ELandmarkAxis::Resonance:  Target.Meta.Resonance   = FMath::Clamp(FMath::Max(Target.Meta.Resonance,  FloorValue), 0.0f, 1.0f); break;
    default: break;
    }
}

// Ленивая загрузка из /Game/Herbalist/Data/DT_Landmarks (2026-09-02, см.
// комментарий у файла выше и у GetAmbientEntityDefinitions() в
// AmbientEntityTypes.h -- тот же паттерн). LogTemp, не HerbalistLogChannels.h
// категория -- эта функция inline, компилируется в любой модуль, который
// её вызывает (включая ProjectHerbalistTests через новый коммандлет),
// категории без API-экспорта дают LNK2001 при линковке из чужого модуля
// (найдено и починено на юните 1/3, тот же урок применён здесь заранее).
inline const TArray<FLandmarkDefinition>& GetLandmarkDefinitions()
{
    static const TArray<FLandmarkDefinition> Definitions = []()
    {
        check(IsInGameThread());   // LoadObject не потокобезопасен

        TArray<FLandmarkDefinition> Defs;
        UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_Landmarks"));
        if (!Table)
        {
            UE_LOG(LogTemp, Error, TEXT("GetLandmarkDefinitions: не удалось загрузить DT_Landmarks -- Основной ранг бестиария будет пуст"));
            return Defs;
        }
        Table->AddToRoot();

        TArray<FLandmarkDefinition*> Rows;
        Table->GetAllRows(TEXT("GetLandmarkDefinitions"), Rows);
        Defs.Reserve(Rows.Num());
        for (const FLandmarkDefinition* Row : Rows)
        {
            if (Row) Defs.Add(*Row);
        }
        Defs.Sort([](const FLandmarkDefinition& A, const FLandmarkDefinition& B) { return A.SortOrder < B.SortOrder; });
        return Defs;
    }();
    return Definitions;
}

inline const FLandmarkDefinition* FindLandmarkDefinition(FName EntityID)
{
    for (const FLandmarkDefinition& D : GetLandmarkDefinitions())
    {
        if (D.EntityID == EntityID) return &D;
    }
    return nullptr;
}
