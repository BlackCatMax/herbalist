// LegendaryEntityTypes.h
//
// Легендарный ранг бестиария (16_Entity_Manifestation.md §16.4): "не
// патрулируют, не спавнятся регулярно — редкое событие, отражающее
// совокупное состояние биома/графа, не конкретную клетку". До 2026-08-29
// единственный реализованный Легендарный (Берегиня) жил как захардкоженный
// блок в GridWorldManagerEntities.cpp (два независимых пути: HistoryPurity
// клетки ИЛИ Restoration капища) — тот же долг, что уже был у Низшего до
// AmbientEntityTypes.h и у Основного до LandmarkTypes.h. Берегиня НЕ
// перенесена сюда (её HistoryPurity — per-клеточный аккумулятор, заведённый
// только для Речной поймы, не общий сигнал уровня биом-графа) — этот файл
// обобщает остальные 16 карточек §16.4, используя сигнал уровня графа
// (BiomeGraphSubsystem, MorokField узла), а не отдельный per-клеточный
// накопитель на каждое существо.
//
// §16.4 называет два полюса:
//   - Опасный (Болотный царь, Лихо Одноглазое, Водяной царь, Суховей):
//     "всплеск MorokField узла ЛИБО серия недавних Catastrophe". Серия
//     Catastrophe не реализована (нет счётчика недавних исходов по биому) —
//     упрощено до одного пути, MorokField-спайк, тем же принципом, что уже
//     применён к "заброшенному жилью" Злыдней (упрощение вместо блокировки).
//   - Благой (Дуб-старец, Гамаюн, Алконост, Мать-Сыра-Земля, Индрик-зверь,
//     Волот, Полкан, Вольга, Дубыня, жар-птица): "устойчиво низкий Distortion
//     узла ИЛИ высокая Restoration капища поблизости" — MorokField узла как
//     прокси "Distortion узла" (тот же прокси, что уже использован для
//     Болотных огней в AmbientEntityTypes.h), второй путь — тот же
//     HerbalistCore::Shrine::GetInfluenceAt, что уже использует Берегиня.
//   - Сирин и Кикимора-владычица — "зеркальны благому триггеру": та же
//     MorokField-проверка, что у опасного полюса (высокий, не низкий), но
//     без агрессивного эффекта — хранители испорченного состояния, не
//     монстры. Заведены с Pole::Malign и мягким эффектом (не Corruption-
//     уроном), различие только в EffectAxis, не в механике триггера.
//
// Награда (редкий ингредиент/разовое видение) НЕ реализована — только
// проявление (ManifestedEntityID) и TargetState-эффект, тот же вертикальный
// срез, что уже применён ко всем прежним пачкам бестиария в этой сессии.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Entities/LandmarkTypes.h"   // ELandmarkAxis/ApplyLandmarkAxisNudge — тот же словарь осей, не новый
#include "LegendaryEntityTypes.generated.h"

UENUM()
enum class ELegendaryPole : uint8
{
    Benign,   // низкий MorokField узла ИЛИ высокая Restoration капища рядом
    Malign    // высокий MorokField узла (спайк)
};

USTRUCT()
struct FLegendaryEntityDefinition
{
    GENERATED_BODY()

    UPROPERTY() FName EntityID;
    UPROPERTY() EBiomeType Biome = EBiomeType::Bog;
    UPROPERTY() bool bLandOnly = false;
    UPROPERTY() bool bWaterOnly = false;

    UPROPERTY() ELegendaryPole Pole = ELegendaryPole::Benign;

    // Malign: MorokField(узла) > MorokThreshold.
    // Benign: MorokField(узла) < MorokThreshold (потолок, "устойчиво низкий").
    UPROPERTY() float MorokThreshold = 0.3f;

    // Второй путь, только для Benign — тот же капищный сигнал, что у
    // Берегини. bHasShrinePath=false отключает путь целиком (не 0 -- ноль
    // Restoration формально мог бы пройти порог 0, отдельный флаг честнее).
    UPROPERTY() bool bHasShrinePath = false;
    UPROPERTY() float ShrineThreshold = 0.7f;

    // До двух осей эффекта — тот же словарь ELandmarkAxis/ApplyLandmarkAxisNudge,
    // что уже применяет Основной ранг (LandmarkTypes.h), не отдельный набор.
    UPROPERTY() ELandmarkAxis EffectAxis = ELandmarkAxis::None;
    UPROPERTY() float EffectRate = 0.0f;
    UPROPERTY() ELandmarkAxis EffectAxis2 = ELandmarkAxis::None;
    UPROPERTY() float EffectRate2 = 0.0f;

    // Физическое представление (2026-08-30) — пусто = базовый
    // ALegendaryEntityActor. См. комментарий у одноимённого поля в
    // AmbientEntityTypes.h.
    UPROPERTY() TSubclassOf<class AHerbalistEntityActor> ActorClass;
};

// Статический реестр — тот же паттерн, что GetAmbientEntityDefinitions()/
// GetLandmarkDefinitions().
inline const TArray<FLegendaryEntityDefinition>& GetLegendaryEntityDefinitions()
{
    static const TArray<FLegendaryEntityDefinition> Definitions = []()
    {
        TArray<FLegendaryEntityDefinition> Defs;

        // --- Опасный полюс: MorokField-спайк узла биома ---

        // Болотный царь (Болото, Смертельная): "мощь порчи и разложения" ->
        // Corruption, тяжелее любого низшего/основного curse в этом биоме.
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Болотный царь"));
            D.Biome = EBiomeType::Bog;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.75f;
            D.EffectAxis = ELandmarkAxis::Corruption; D.EffectRate = 0.03f;
            Defs.Add(D);
        }

        // Лихо Одноглазое (Болото, Смертельная): "меняет судьбу" -- в модели
        // нет судьбы/удачи как оси, ближайший честный эквивалент --
        // дезориентация/хаос итога (Distortion), не выдуманная новая ось.
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Лихо Одноглазое"));
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.8f;
            D.EffectAxis = ELandmarkAxis::Distortion; D.EffectRate = 0.03f;
            Defs.Add(D);
        }

        // Водяной царь (Речная пойма, ВОДА, Смертельная): "власть над водой
        // и погодой" -> Distortion (буря/хаос стихии) + Stability-- (никакой
        // твёрдой опоры под ногами рядом с ним).
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Водяной царь"));
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.75f;
            D.EffectAxis = ELandmarkAxis::Distortion; D.EffectRate = 0.025f;
            D.EffectAxis2 = ELandmarkAxis::Stability; D.EffectRate2 = -0.015f;
            Defs.Add(D);
        }

        // Суховей (Степь, Высокая): "иссушение и увядание" -> Corruption
        // (порча урожая/растительности, тот же язык, что у Суховеек §16.2,
        // только на порядок сильнее и на уровне графа, не клетки).
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Суховей"));
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Corruption; D.EffectRate = 0.02f;
            Defs.Add(D);
        }

        // --- Благой полюс: устойчиво низкий MorokField узла ИЛИ капище рядом ---

        // Дуб-старец (Широколиств. лес, Нейтральный): "древняя мудрость и
        // непоколебимость" -> Stability.
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Дуб-старец"));
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Stability; D.EffectRate = 0.015f;
            Defs.Add(D);
        }

        // Гамаюн (Лесостепь): вещая птица истины -- "снимает морок,
        // показывает правду" -> Purity (расчищает искажение до ясности,
        // тот же язык, что и у остальных "очищающих" эффектов проекта).
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Гамаюн"));
            D.Biome = EBiomeType::ForestSteppe;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Purity; D.EffectRate = 0.015f;
            Defs.Add(D);
        }

        // Алконост (Степь): райская птица радости -- "исцеление душевных
        // ран" -> Purity (тот же язык оздоровления, что у Гамаюна, разная
        // птица квартета, тот же ближайший честный эквивалент "радости").
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Алконост"));
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Purity; D.EffectRate = 0.012f;
            Defs.Add(D);
        }

        // Мать-Сыра-Земля (степная, Нейтральный): "земля-хранительница,
        // общий стабилизатор и очиститель" -> Stability + Purity, единый
        // "материнский" эффект, тот же двухосевой паттерн, что уже есть у
        // Полевика/Переплута/Мокоши (капище) в этой сессии.
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Мать-Сыра-Земля"));
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.65f;
            D.EffectAxis = ELandmarkAxis::Stability; D.EffectRate = 0.012f;
            D.EffectAxis2 = ELandmarkAxis::Purity;   D.EffectRate2 = 0.008f;
            Defs.Add(D);
        }

        // Индрик-зверь (Тайга): "зверь-владыка, первозданная мощь и связь
        // с природой" -> Potency + Nature (Direction), сила и дикость разом.
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Индрик-зверь"));
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Potency; D.EffectRate = 0.012f;
            D.EffectAxis2 = ELandmarkAxis::Nature; D.EffectRate2 = 0.01f;
            Defs.Add(D);
        }

        // Волот (Тундра): "древний великан-страж, незыблемая опора и
        // стойкость к морозу" -> Stability, тот же язык, что уже есть у
        // Хозяина Севера (капище той же Тундры), другой масштаб (Легендарный).
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Волот"));
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Stability; D.EffectRate = 0.015f;
            Defs.Add(D);
        }

        // Полкан (Лесостепь): полуконь-получеловек, воинская сила -> Body.
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Полкан"));
            D.Biome = EBiomeType::ForestSteppe;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Body; D.EffectRate = 0.012f;
            Defs.Add(D);
        }

        // Вольга (Тайга): богатырь-оборотень, сила и превращение -> Body
        // (сама способность к превращению не моделируется отдельной осью --
        // тот же честный отказ от выдуманной оси, что у Лихо Одноглазого).
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Вольга"));
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Body; D.EffectRate = 0.012f;
            Defs.Add(D);
        }

        // Дубыня (Смеш. лес): богатырь-древоборец -> Body + Nature, та же
        // пара осей, что у Индрик-зверя (сила + связь с лесом), другой биом.
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Дубыня"));
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Body;   D.EffectRate = 0.01f;
            D.EffectAxis2 = ELandmarkAxis::Nature; D.EffectRate2 = 0.01f;
            Defs.Add(D);
        }

        // жар-птица (Смеш. лес): "перо = удача/богатство" -- в модели нет
        // удачи как оси, ближайший честный эквивалент -- Resonance (та же
        // "прозорливость/везение" трактовка, что уже у Курганных огней,
        // "поиск сокрытого" §16.3).
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("жар-птица"));
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.75f;
            D.EffectAxis = ELandmarkAxis::Resonance; D.EffectRate = 0.015f;
            Defs.Add(D);
        }

        // --- Зеркальный полюс: высокий MorokField, но не хищный эффект ---
        // §16.4: "их проявление привязано не к Restoration, а к глубине
        // искажения... зеркальны благому триггеру: легендарные хранители
        // самого испорченного состояния, не самого чистого". Malign по
        // механике триггера (высокий MorokField), но эффект не Corruption-
        // урон, а Distortion/Resonance -- "забвение", не разрушение.

        // Сирин (Тундра, Нейтральный, но опасна): птица забвения и тоски ->
        // Distortion + Resonance -- "забвение", завершает квартет вещих
        // птиц (Алконост=радость, Гамаюн=истина, Сирин=забвение-тоска,
        // жар-птица=удача, §16.4 "смысловой квартет").
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Сирин"));
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Distortion; D.EffectRate = 0.02f;
            D.EffectAxis2 = ELandmarkAxis::Resonance; D.EffectRate2 = 0.015f;
            Defs.Add(D);
        }

        // Кикимора-владычица (Широколиств. лес, Нейтральный, danger
        // Высокая): "владычица обмана и скрытности" -> Distortion +
        // Resonance, тот же расклад осей, что уже выбран для Кикиморы
        // болотной (капище того же семейства, меньший масштаб).
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Кикимора-владычица"));
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Distortion; D.EffectRate = 0.02f;
            D.EffectAxis2 = ELandmarkAxis::Resonance; D.EffectRate2 = 0.012f;
            Defs.Add(D);
        }

        return Defs;
    }();
    return Definitions;
}

inline const FLegendaryEntityDefinition* FindLegendaryEntityDefinition(FName EntityID)
{
    for (const FLegendaryEntityDefinition& D : GetLegendaryEntityDefinitions())
    {
        if (D.EntityID == EntityID) return &D;
    }
    return nullptr;
}
