// LegendaryEntityTypes.h
//
// Легендарный ранг бестиария (16_Entity_Manifestation.md §16.4): "не
// патрулируют, не спавнятся регулярно — редкое событие, отражающее
// совокупное состояние биома/графа, не конкретную клетку". До 2026-08-29
// единственный реализованный Легендарный (Берегиня) жил как захардкоженный
// блок в GridWorldManagerEntities.cpp (два независимых пути: HistoryPurity
// клетки ИЛИ Restoration капища) — тот же долг, что уже был у Низшего до
// AmbientEntityTypes.h и у Основного до LandmarkTypes.h.
//
// 2026-09-02 (унификация Берегини, ответ на "все сущности должны быть
// равны... без исключений"): этот файл обобщает ВСЕ 17 карточек §16.4,
// включая Берегиню. Механизм триггера — не один, а два, выбираемых полем
// bUsesCellHistoryPurity: (а) сигнал уровня графа (BiomeGraphSubsystem,
// MorokField узла) на ОДНОЙ фиксированной якорной клетке
// (SeedLegendaryAnchors) — 16 исходных карточек; (б) per-клеточный
// аккумулятор Cell.Memory.HistoryPurity, без якоря — ЛЮБАЯ подходящая
// клетка биома независимо проверяется каждый тик (см. GridWorldManagerEntities.cpp,
// цикл по Cells). Берегиня — первая карточка второго типа, но не
// единственно возможная: bUsesCellHistoryPurity заведён так, чтобы
// принять ещё одну такую сущность будущим редактированием DataTable,
// без правки C++.
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
//
// 2026-09-02, юнит 3/3 (последний) миграции бестиария на DataTable —
// тот же паттерн, что AmbientEntityTypes.h (1/3) и LandmarkTypes.h (2/3):
// GetLegendaryEntityDefinitions() ниже лениво грузит
// /Game/Herbalist/Data/DT_LegendaryEntities. Берегиня (см. выше) остаётся
// вне этого файла и вне миграции — свой per-клеточный путь.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Entities/LandmarkTypes.h"   // ELandmarkAxis/ApplyLandmarkAxisNudge — тот же словарь осей, не новый
#include "Engine/DataTable.h"
#include "LegendaryEntityTypes.generated.h"

UENUM()
enum class ELegendaryPole : uint8
{
    Benign,   // низкий MorokField узла ИЛИ высокая Restoration капища рядом
    Malign    // высокий MorokField узла (спайк)
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FLegendaryEntityDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY() FName EntityID;
    UPROPERTY() EBiomeType Biome = EBiomeType::Bog;
    UPROPERTY() bool bLandOnly = false;
    UPROPERTY() bool bWaterOnly = false;

    // Явный порядок регистрации (2026-09-02, тот же приём, что уже
    // FAmbientEntityDefinition/FLandmarkDefinition::SortOrder) -- у
    // Legendary тоже нет задокументированной зависимости от порядка
    // (каждый Def привязан к своему якорю через SeedLegendaryAnchors,
    // не соревнуется за клетку в момент проявления), заведено для
    // единообразия.
    UPROPERTY() int32 SortOrder = 0;

    UPROPERTY() ELegendaryPole Pole = ELegendaryPole::Benign;

    // Malign: MorokField(узла) > MorokThreshold.
    // Benign: MorokField(узла) < MorokThreshold (потолок, "устойчиво низкий").
    // Игнорируется, если bUsesCellHistoryPurity=true (см. поле ниже) —
    // якорный MorokField-путь для таких карточек не используется вовсе.
    UPROPERTY() float MorokThreshold = 0.3f;

    // Второй путь, изначально заведённый только для Berign-полюса 16
    // якорных карточек — тот же капищный сигнал, что и у per-клеточных
    // (bUsesCellHistoryPurity=true) карточек ниже, поле общее для обоих
    // механизмов. bHasShrinePath=false отключает путь целиком (не 0 --
    // ноль Restoration формально мог бы пройти порог 0, отдельный флаг
    // честнее).
    UPROPERTY() bool bHasShrinePath = false;
    UPROPERTY() float ShrineThreshold = 0.7f;

    // 2026-09-02 (унификация Берегини) — переключает Def с якорного
    // MorokField-триггера (16 исходных карточек, по умолчанию) на
    // per-клеточный: клетка сама несёт Cell.Memory.HistoryPurity, якорь
    // не заводится (SeedLegendaryAnchors пропускает такие Def), условие
    // проверяется независимо на КАЖДОЙ подходящей клетке биома каждый
    // тик, не на одной фиксированной. true=Берегиня и любая будущая
    // карточка того же типа.
    UPROPERTY() bool bUsesCellHistoryPurity = false;

    // Порог для per-клеточного пути (см. bUsesCellHistoryPurity выше) --
    // читается только когда bUsesCellHistoryPurity=true. Отдельное поле
    // от MorokThreshold: тот -- порог графового узла, этот -- порог
    // per-клеточного аккумулятора, разные величины по конструкции.
    UPROPERTY() float HistoryPurityThreshold = 0.75f;

    // До двух осей эффекта — тот же словарь ELandmarkAxis, что уже
    // применяет Основной ранг (LandmarkTypes.h), не отдельный набор.
    UPROPERTY() ELandmarkAxis EffectAxis = ELandmarkAxis::None;
    UPROPERTY() float EffectRate = 0.0f;
    UPROPERTY() ELandmarkAxis EffectAxis2 = ELandmarkAxis::None;
    UPROPERTY() float EffectRate2 = 0.0f;

    // 2026-09-02 (унификация Берегини) — переключает применение
    // EffectAxis/EffectRate: false (16 исходных карточек) -- аддитивный
    // нудж ApplyLandmarkAxisNudge (EffectRate -- скорость/сек). true --
    // EffectRate читается как ЗНАЧЕНИЕ ПОЛА (floor), ApplyLandmarkAxisFloor
    // (LandmarkTypes.h) не даёт оси опуститься ниже него, пока карточка
    // проявлена -- мгновенное свойство, не скорость. Берегиня: floor
    // Purity на 0.9. EffectAxis2/EffectRate2 всегда аддитивны (второй
    // floor не заведён -- нет карточки, которой он нужен).
    UPROPERTY() bool bFloorEffect = false;

    // Физическое представление (2026-08-30) — пусто = базовый
    // ALegendaryEntityActor. См. комментарий у одноимённого поля в
    // AmbientEntityTypes.h.
    UPROPERTY() TSubclassOf<class AHerbalistEntityActor> ActorClass;
};

// Ленивая загрузка из /Game/Herbalist/Data/DT_LegendaryEntities
// (2026-09-02) -- тот же паттерн, что GetAmbientEntityDefinitions()/
// GetLandmarkDefinitions(), см. подробное обоснование в AmbientEntityTypes.h.
// LogTemp, не HerbalistLogChannels.h категория -- та же причина (LNK2001,
// найдено на юните 1/3): inline-функция компилируется и в
// ProjectHerbalistTests через новый коммандлет.
inline const TArray<FLegendaryEntityDefinition>& GetLegendaryEntityDefinitions()
{
    static const TArray<FLegendaryEntityDefinition> Definitions = []()
    {
        check(IsInGameThread());   // LoadObject не потокобезопасен

        TArray<FLegendaryEntityDefinition> Defs;
        UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_LegendaryEntities"));
        if (!Table)
        {
            UE_LOG(LogTemp, Error, TEXT("GetLegendaryEntityDefinitions: не удалось загрузить DT_LegendaryEntities -- Легендарный ранг бестиария будет пуст (кроме Берегини, она отдельно)"));
            return Defs;
        }
        Table->AddToRoot();

        TArray<FLegendaryEntityDefinition*> Rows;
        Table->GetAllRows(TEXT("GetLegendaryEntityDefinitions"), Rows);
        Defs.Reserve(Rows.Num());
        for (const FLegendaryEntityDefinition* Row : Rows)
        {
            if (Row) Defs.Add(*Row);
        }
        Defs.Sort([](const FLegendaryEntityDefinition& A, const FLegendaryEntityDefinition& B) { return A.SortOrder < B.SortOrder; });
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
