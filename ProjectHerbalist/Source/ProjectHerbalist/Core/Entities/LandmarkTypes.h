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
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
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

USTRUCT()
struct FLandmarkDefinition
{
    GENERATED_BODY()

    UPROPERTY() FName EntityID;
    UPROPERTY() EBiomeType Biome = EBiomeType::ForestSteppe;

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

// Статический реестр — тот же паттерн, что GetAmbientEntityDefinitions()
// (AmbientEntityTypes.h) и HerbalistCore::Zaryana::GetMemoryFragmentDefinitions().
inline const TArray<FLandmarkDefinition>& GetLandmarkDefinitions()
{
    static const TArray<FLandmarkDefinition> Definitions = []()
    {
        TArray<FLandmarkDefinition> Defs;

        // Полевик (Лесостепь) — уже был реализован до 2026-08-29, числа не
        // менялись, только перенесены сюда из захардкоженного блока.
        // "Благословлённое зерно" (08_Content/бестиарий: повышенная Potency
        // и Purity); "порча посевов, наведение усталости" -> Stability.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Полевик"));
            D.Biome = EBiomeType::ForestSteppe;
            D.BlessAxis = ELandmarkAxis::Potency;  D.BlessRate = 0.01f;
            D.BlessAxis2 = ELandmarkAxis::Purity;  D.BlessRate2 = 0.005f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.02f;
            Defs.Add(D);
        }

        // Аука (Тайга) — "мох с дерева, где он сидел, усиливает разум и
        // скрытность" -> Mind. Разгневанный уводит с тропы шутки ради ->
        // дезориентация, Distortion.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Аука"));
            D.Biome = EBiomeType::Taiga;
            D.BlessAxis = ELandmarkAxis::Mind; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.02f;
            Defs.Add(D);
        }

        // Дух Медведя (Тайга) — "Body" по прямому тексту §16.3 самого GDD,
        // подтверждено карточкой (d_manifest[Body]=0.9, доминирующая ось).
        // Разгневанный медвежий дух — самый опасный ("danger: Высокая") из
        // всей пачки, curse на Stability, не мельче остальных.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Дух Медведя"));
            D.Biome = EBiomeType::Taiga;
            D.BlessAxis = ELandmarkAxis::Body; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.025f;
            Defs.Add(D);
        }

        // Хозяин Севера (Тундра) — ледяной артефакт даёт "защиту от
        // обморожения, выносливость" -> Stability. Разгневанный испытывает
        // холодом и страхом путника -> Distortion.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Хозяин Севера"));
            D.Biome = EBiomeType::Tundra;
            D.BlessAxis = ELandmarkAxis::Stability; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.02f;
            Defs.Add(D);
        }

        // Гуменник (Широколиств. лес) — "благословенное зерно повышает
        // Potency зелий изобилия" -> Potency. Хозяйство без ухода портится
        // без явного мех. акта поджога (декоративная угроза в тексте) ->
        // Stability как общий "запущенное хозяйство" эффект.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Гуменник"));
            D.Biome = EBiomeType::BroadleafForest;
            D.BlessAxis = ELandmarkAxis::Potency; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.02f;
            Defs.Add(D);
        }

        // Овинник (Широколиств. лес) — "пепел из его овина повышает Purity
        // и защиту от огня" -> Purity. Обиженный овинник — угроза пожара ->
        // Corruption как "порча хозяйства через недобрый огонь".
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Овинник"));
            D.Biome = EBiomeType::BroadleafForest;
            D.BlessAxis = ELandmarkAxis::Purity; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Corruption; D.CurseRate = 0.02f;
            Defs.Add(D);
        }

        // Кикимора болотная (Болото) — единственная неоднозначная фигура
        // пачки: собственная карточка называет её "дар" (волос/слюна)
        // ингредиентом для зелий обмана, ПОВЫШАЮЩИМ Distortion уже в
        // благосклонном варианте — не Purity/Potency, как у прочих
        // "хозяйственных" духов. Bless -> Resonance (её колдовская
        // прозорливость, не морок впрямую); curse -> Distortion сильнее
        // обычного (morok_affinity 0.6, выше среднего по пачке) —
        // она уже наполовину созданию Морока принадлежит, недовольная
        // отпускает его без остатка.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Кикимора болотная"));
            D.Biome = EBiomeType::Bog;
            D.BlessAxis = ELandmarkAxis::Resonance; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.025f;
            Defs.Add(D);
        }

        // Переплут (Степь) — "шерсть/молоко благословлённого скота
        // повышают Body и Nature" -- единственный второй случай (после
        // Полевика) с двумя явно названными осями в самой карточке.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Переплут"));
            D.Biome = EBiomeType::Steppe;
            D.BlessAxis = ELandmarkAxis::Body; D.BlessRate = 0.008f;
            D.BlessAxis2 = ELandmarkAxis::Nature; D.BlessRate2 = 0.008f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.02f;
            Defs.Add(D);
        }

        // Бродницы (Речная пойма) — либо безопасно проводят через брод,
        // либо заманивают в глубину. Bless -> Stability (уверенная,
        // безопасная переправа); curse -> Distortion (заведёт на глубину,
        // потеря ориентации).
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Бродницы"));
            D.Biome = EBiomeType::Floodplain;
            D.BlessAxis = ELandmarkAxis::Stability; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.02f;
            Defs.Add(D);
        }

        // Боровик (Смеш. лес) — "грибы, собранные с его благословения,
        // отличаются повышенной Potency" -> Potency.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Боровик"));
            D.Biome = EBiomeType::MixedForest;
            D.BlessAxis = ELandmarkAxis::Potency; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.02f;
            Defs.Add(D);
        }

        // Луговой (Смеш. лес) — "трава с его луга отличается высокой
        // Purity" -> Purity.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Луговой"));
            D.Biome = EBiomeType::MixedForest;
            D.BlessAxis = ELandmarkAxis::Purity; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.015f;
            Defs.Add(D);
        }

        // Курганники (Лесостепь, изначально §16.2 — требовал подношения,
        // значит на деле §16.3) — "пыль кургана для оберегов и связи с
        // предками" -> Spirit (духовная ось, не физическая защита).
        // Потревоживших курган без спроса карточка прямо проклинает
        // болезнью -> Corruption.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Курганники"));
            D.Biome = EBiomeType::ForestSteppe;
            D.BlessAxis = ELandmarkAxis::Spirit; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Corruption; D.CurseRate = 0.025f;
            Defs.Add(D);
        }

        // Жердяи (Широколиств. лес, изначально §16.2) — "оберег из щепки
        // одержимого шеста" -> Stability (защитная тема, они охраняют
        // границу владения). Пугают, не вредят напрямую -> Distortion,
        // мягче Курганников (danger: Низкая у Жердяев против Средней).
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Жердяи"));
            D.Biome = EBiomeType::BroadleafForest;
            D.BlessAxis = ELandmarkAxis::Stability; D.BlessRate = 0.008f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.012f;
            Defs.Add(D);
        }

        // Курганные огни (Степь, изначально §16.2) — "связь с предками и
        // поиск сокрытого" -> Resonance (прозорливость/поиск, не прямая
        // духовная ось, как у Курганников — разная механика для двух
        // родственных по лору, но разных по карточке сущностей). Могут
        // завести в ловушку -> Distortion.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Курганные огни"));
            D.Biome = EBiomeType::Steppe;
            D.BlessAxis = ELandmarkAxis::Resonance; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.02f;
            Defs.Add(D);
        }

        // Домовой (жилище игрока, DESIGN_Community_And_Homestead.md §2.1,
        // 2026-08-31) — не биом, регистрируется напрямую на клетке
        // AAlchemyTableActor (bManualRegistrationOnly=true, SeedTestLandmarks
        // пропускает). Хороший Respect защищает дом — Stability, тот же
        // смысл, что уже даёт капищам устойчивость к порче (§12.10). Плохой
        // — мелкие пакости, Corruption. При-сильно плохом (ниже -0.6) —
        // второй, более резкий удар (Stability вниз тоже) — эскалация в
        // домашнюю Кикимору (собрана этой же сессией,
        // DESIGN_Brewing_Situations_And_Lore.md), не отдельная сущность.
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Домовой"));
            D.bManualRegistrationOnly = true;
            D.BlessAxis = ELandmarkAxis::Stability; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Corruption; D.CurseRate = 0.015f;
            D.AggravatedCurseAxis = ELandmarkAxis::Stability;
            D.AggravatedCurseRate = -0.02f;
            D.AggravatedCurseThreshold = -0.6f;
            Defs.Add(D);
        }

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
