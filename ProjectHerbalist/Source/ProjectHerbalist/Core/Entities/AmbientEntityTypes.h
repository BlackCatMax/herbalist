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
// Сознательно НЕ вынесено в UDataTable/DA_*.uasset (в отличие от биомов и
// ингредиентов): определений уже 17 (2026-08-29), они меняются вместе с
// кодом (новый EAmbientTriggerAxis требует правки C++ всё равно), и это тот
// же принцип, что уже применён к MemoryFragmentDefinitions.h — простой
// статический реестр, а не полноценный ассет-пайплайн, пока карточек мало.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
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

USTRUCT()
struct FAmbientEntityDefinition
{
    GENERATED_BODY()

    UPROPERTY() FName EntityID;
    UPROPERTY() EBiomeType Biome = EBiomeType::Bog;

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
};

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

// Статический реестр — тот же паттерн, что HerbalistCore::Zaryana::
// GetMemoryFragmentDefinitions() (MemoryFragmentDefinitions.h).
inline const TArray<FAmbientEntityDefinition>& GetAmbientEntityDefinitions()
{
    static const TArray<FAmbientEntityDefinition> Definitions = []()
    {
        TArray<FAmbientEntityDefinition> Defs;

        // Гнильники (Болото, земля): Corruption > 0.6 -> Corruption++, Purity--.
        // Перенесено из прежнего захардкоженного блока без изменения чисел —
        // регрессия Herbalist.Bistability.* проверяет именно эти пороги.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Гнильники"));
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Corruption;
            D.TriggerThreshold = 0.6f;
            D.bTriggerAbove = true;
            D.CorruptionRate = 0.01f;
            D.PurityRate = -0.005f;
            Defs.Add(D);
        }

        // Моховые духи (Тайга, земля, 16_Entity_Manifestation §16.2): "Purity
        // клетки высокая -> Stability++, Purity++" — единственный "улучшающий"
        // низший по спецификации; зеркало Гнильников (самоусиливающееся
        // оздоровление вместо самоусиливающейся порчи).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Моховые духи"));
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Purity;
            D.TriggerThreshold = 0.75f;
            D.bTriggerAbove = true;
            D.PurityRate = 0.01f;
            D.StabilityRate = 0.01f;
            Defs.Add(D);
        }

        // Степные огни (Степь, земля, §16.2): "сумерки/ночь, открытый биом ->
        // Distortion++, дезориентация восприятия" — блуждающие огни степного
        // фольклора, заводящие путника в темноте. Чисто ночной триггер, без
        // Meta-оси (TriggerAxis::None).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Степные огни"));
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.DistortionRate = 0.008f;
            Defs.Add(D);
        }

        // Кувшинкины духи (Речная пойма, земля, §16.2): "заросли, ночь ->
        // Resonance++, сонливость" — заросли/берег читаем как земляную
        // кромку поймы, не саму воду (вода поймы — территория Берегини,
        // §16.4; bLandOnly исключает коллизию биома без явного порядка
        // приоритетов). Чисто ночной триггер, как у Степных огней.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Кувшинкины духи"));
            D.Biome = EBiomeType::Floodplain;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.ResonanceRate = 0.01f;
            Defs.Add(D);
        }

        // Ледяные духи (Тундра, земля, §16.2): "низкая температура (сезон,
        // §15.4 Зима) -> заморозка, временное снижение Magnitude". Первое
        // существо, завязанное на сезон, а не на время суток — разблокировано
        // после того, как сезоны получили v1-реализацию 2026-08-24.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Ледяные духи"));
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresSeason = true;
            D.RequiredSeason = ESeason::Winter;
            D.MagnitudeRate = -0.01f;
            Defs.Add(D);
        }

        // Суховейки (Степь, земля, §16.2): "засушливый сезон (§15.4 Лето) ->
        // иссушение, Magnitude--". Лето в GetSeason() нейтрально для
        // StressRecoveryMultiplier (у него нет числа в спецификации для ЭТОГО
        // эффекта), но это не мешает Лету быть триггером для ДРУГОГО,
        // независимого потребителя того же GetSeason() — не одно и то же
        // решение, не противоречие.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Суховейки"));
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresSeason = true;
            D.RequiredSeason = ESeason::Summer;
            D.MagnitudeRate = -0.008f;
            Defs.Add(D);
        }

        // Омутные огни (Речная пойма, ВОДА, §16.2): "глубокий омут,
        // безлунная ночь -> Distortion↑ сильно, ловушка восприятия".
        // Глубина не моделируется (см. комментарий у bRequiresMoonPhase
        // выше) — Новолуние+Ночь читаются буквально, оба уже существующие
        // сигналы (GetMoonPhase/IsNight), не прокси. Зарегистрировано ДО
        // Русалок ниже (тот же биом+вода+ночь) намеренно — Русалки условие
        // строго слабее (просто ночь, без фазы луны), при первом заявленном
        // ранге 0 выигрывает первый по порядку в реестре: без этого порядка
        // Омутные огни никогда не получили бы свою редкую, более узкую
        // новолунную ночь, Русалки забирали бы клетку каждую ночь раньше них.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Омутные огни"));
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.bRequiresMoonPhase = true;
            D.RequiredMoonPhase = EMoonPhase::NewMoon;
            D.DistortionRate = 0.02f;   // "сильно", выше остальных Distortion-нуджей поймы
            Defs.Add(D);
        }

        // Русалки (Речная пойма, ВОДА, §16.2 — 2026-08-29, по решению
        // пользователя). Единственная сущность бестиария этой сессии с
        // `behavior: Враждебный` в карточке (04_Compendium/Бестиарий/
        // Русалки.md) — все прочие "хозяева"/низшие нейтральны. Симметричный
        // §16.3-механизм "хозяина" (благословение за уважение) ей не
        // подходит: фольклорно она не хранитель места, а опасность, которую
        // избегают, не приручают через подношение. Заведена как амбиентная
        // зона того же класса, что Омутные огни (тоже вода Речной поймы,
        // тоже "ловушка восприятия") — заманивает песней и щекоткой,
        // Distortion++, ночь. bWaterOnly (не bLandOnly) — Кувшинкины духи
        // уже заняли земляную кромку той же поймы, коллизии нет; если на
        // клетке одновременно высокая Restoration капища даёт Берегиню
        // (Легендарный, приоритет 2 против 0 у Русалок) — Берегиня
        // забирает ManifestedEntityID, Русалки в этот тик не проявляются —
        // осознанное, не случайное следствие: благословенный участок реки
        // не должен кишеть опасностью одновременно.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Русалки"));
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.DistortionRate = 0.012f;
            Defs.Add(D);
        }

        // Ржавые духи (Болото, земля, §16.2): карточка описывает "Stability
        // клетки низкая -> порча инструмента". Задумывался как location-based
        // порча предметов, читаемая напрямую HerbalistInventoryComponent —
        // отменено правкой пользователя 2026-08-29 ("травы портятся сами по
        // себе... на сохранность влияют сами контейнеры хранения", см.
        // EStorageContainerType в HerbalistInventoryComponent.h). Порог
        // проявления и биом оставлены (существо манифестируется по §16.2),
        // сам эффект — заглушка на будущий редизайн, не TargetState-нудж.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Ржавые духи"));
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Stability;
            D.TriggerThreshold = 0.3f;
            D.bTriggerAbove = false;   // низкая Stability, не высокая
            Defs.Add(D);
        }

        // Водяные бесы (Речная пойма, ВОДА, §16.2): карточка описывает
        // "мутная вода -> мелкая порча снаряжения" (мутная вода = высокий
        // Distortion воды). Эффект порчи предметов отменён вместе с Ржавыми
        // духами (см. комментарий выше) — существо манифестируется, эффекта
        // пока нет.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Водяные бесы"));
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Distortion;
            D.TriggerThreshold = 0.5f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }

        // Злыдни (Широколиств. лес, земля, §16.2): карточка описывает
        // "заброшенное жильё, накопленный HarvestStress -> порча инвентаря".
        // "Заброшенное жильё" как отдельная сущность (конкретный дом-
        // landmark) в модели данных не существует — упрощено до истощённого
        // участка леса (HarvestStress клетки), той же оси, что уже называет
        // карточка. Эффект порчи предметов отменён вместе с двумя существами
        // выше (см. комментарий у Ржавых духов).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Злыдни"));
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::HarvestStress;
            D.TriggerThreshold = 0.6f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }

        // Трясинные духи (Болото, земля, §16.2): "Nature-ось доминирует ->
        // замедление сбора". "Замедление скорости сбора" — отдельный
        // модификатор темпа действия игрока, которого сейчас нет в модели
        // (Rate-поля ниже — все TargetState-нуджи, не темп сбора) — эффект
        // намеренно не реализован, только проявление, тот же принцип
        // заглушки, что уже применён к Ржавым духам/Водяным бесам/Злыдням.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Трясинные духи"));
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Nature;
            D.TriggerThreshold = 0.4f;   // Direction нормализован по сумме (NormalizeSum) -- 0.4 из 1.0 на 4 оси уже заметно доминирует
            D.bTriggerAbove = true;
            Defs.Add(D);
        }

        // Болотные огни (Болото, земля, §16.2): "Morok-поле высокое, ночь ->
        // Resonance++, Distortion++". MorokField не читается напрямую в этом
        // реестре (он на уровне BiomeGraphSubsystem, не клетки) — Distortion
        // клетки уже служит проверенным прокси Морока (тот же принцип, что
        // ApplyBiomeInfluences использует Distortion как канал Морока).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Болотные огни"));
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Distortion;
            D.TriggerThreshold = 0.5f;
            D.bTriggerAbove = true;
            D.bRequiresNight = true;
            D.ResonanceRate = 0.01f;
            D.DistortionRate = 0.006f;
            Defs.Add(D);
        }

        // Шишиги (Смешанный лес, земля, §16.2): "овраг/куст, сумерки ->
        // испуг, визуальный дебафф восприятия, без мех. вреда". Чисто
        // Perception-эффект (как Морочники/Шептуны), не TargetState-нудж —
        // только проявление, сам испуг ещё не подключен к слою восприятия.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Шишиги"));
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresDusk = true;
            Defs.Add(D);
        }

        // Древесные огни (Смешанный лес, земля, §16.2): "старое дерево, ночь
        // -> декоративный". §16.2 сам называет тип "декоративный" — тест на
        // существо без мех. эффекта вовсе, как Снежные огни ниже.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Древесные огни"));
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            Defs.Add(D);
        }

        // Чащобные духи (Тайга, земля, §16.2): "Nature экстремум -> защитный
        // дебафф при вторжении в нетронутую клетку". "Вторжение в нетронутую
        // клетку" как событие (не просто порог) не моделируется — только
        // проявление по тому же Nature-экстремуму, что Трясинные духи выше,
        // без эффекта (заглушка, тот же принцип).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Чащобные духи"));
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Nature;
            D.TriggerThreshold = 0.45f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }

        // Снежные огни (Тундра, земля, §16.2): "ясная ночь -> декоративный".
        // "Ясная" (безоблачная) — сигнал погоды, которой в проекте ещё нет
        // (§15.7) — упрощено до простого ночного триггера, тот же принцип,
        // что уже применён к Ледяным духам до появления сезонов. §16.2 сам
        // называет тип "тест на декоративный" — существо без эффекта, только
        // проявление.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Снежные огни"));
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            Defs.Add(D);
        }

        // Плескуны (Речная пойма, ВОДА, §16.2): "мелководье -> декоративный,
        // Purity нейтрален". "Мелководье" (глубина) не моделируется как
        // атрибут клетки -- нет условия по времени/сезону в самой карточке,
        // а check() ниже требует хоть какой-то гейт. Purity >= 0.0 --
        // тривиально истинно почти всегда (Meta.Purity клампится в [0,1]),
        // честный тонкий гейт вместо выдумывания несуществующего условия:
        // Плескуны почти всегда на месте, что и требует "декоративный".
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Плескуны"));
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Purity;
            D.TriggerThreshold = 0.0f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }

        // Шептуны (Тундра, земля, §16.2): "открытое пространство, искажает
        // тултип (кандидат на PerceiveValue, §16.5)". Тот же класс, что
        // Морочники -- не TargetState-эффект, а искажение восприятия,
        // ещё не подключенное к бестиарию (§16.6). "Открытое пространство"
        // не моделируется отдельным атрибутом -- тот же честный тонкий
        // гейт, что уже у Плескунов (Stability >= 0, практически всегда).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Шептуны"));
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Stability;
            D.TriggerThreshold = 0.0f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }

        // Подпольники (Широколиств. лес, земля, §16.2): "подпол/старый дом,
        // предупреждающий сигнал (UI-хук, не дебафф)". Тот же HarvestStress-
        // прокси "заброшенности", что уже применён к Злыдням (§16.2, третья
        // пачка) -- порог НИЖЕ злыдневского (0.4 против 0.6), читается как
        // ранняя стадия того же упадка: сперва предупреждающий Подпольник,
        // затем, если ничего не изменилось, настоящая порча от Злыдней.
        // UI-хук сам не реализован, только проявление.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Подпольники"));
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::HarvestStress;
            D.TriggerThreshold = 0.4f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }

        // Стукачи (Широколиств. лес, земля, §16.2): "перед крупным мировым
        // событием (Bifurcation), предвестник". Счётчика недавних
        // Catastrophe по биому нет (та же дыра, что у опасного полюса
        // Легендарного, §16.4 -- честно упрощено там же) -- Distortion как
        // прокси "риск срыва", тот же приём, что уже у Болотных огней/
        // Легендарного malign-полюса.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Стукачи"));
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Distortion;
            D.TriggerThreshold = 0.6f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }

        // Пеньковые (Тайга, земля, §16.2): "старый нетронутый участок ->
        // маскировка ресурса в S_Perceived". "Нетронутый" читается как
        // обратное Злыдням -- НИЗКИЙ HarvestStress (никогда не собирали),
        // не новое поле "возраст". Маскировка (Perception-эффект) не
        // подключена, только проявление.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Пеньковые"));
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::HarvestStress;
            D.TriggerThreshold = 0.1f;
            D.bTriggerAbove = false;
            Defs.Add(D);
        }

        // Межевые (Лесостепь, земля, §16.2): "клетка на границе биомов
        // графа -> Nature↑". Единственный реально проверенный (не
        // приближённый) гейт этой пачки -- bRequiresBiomeBorder читает
        // настоящих соседей клетки в сетке (тот же приём, что уже применён
        // к Пограничному капищу, BiomeGraphSubsystem.cpp), не прокси.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Межевые"));
            D.Biome = EBiomeType::ForestSteppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresBiomeBorder = true;
            D.NatureRate = 0.01f;
            Defs.Add(D);
        }

        for (const FAmbientEntityDefinition& D : Defs)
        {
            check(D.TriggerAxis != EAmbientTriggerAxis::None || D.bRequiresNight || D.bRequiresSeason || D.bRequiresDusk || D.bRequiresBiomeBorder);
        }
        return Defs;
    }();
    return Definitions;
}
