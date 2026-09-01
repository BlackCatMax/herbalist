// ArtifactTypes.h
//
// Артефакты Легендарных сущностей (21_Journey_And_Artifacts.md §21.3-21.4,
// 2026-09-01). Тот же паттерн статического реестра, что LegendaryEntityTypes.h/
// LandmarkTypes.h/AmbientEntityTypes.h.
//
// Восемь строк ровно по таблице §21.3. Гребень (Берегиня) — LegendaryEntityID
// пуст: Берегиня НЕ в LegendaryEntityTypes.h (см. комментарий в её шапке —
// HistoryPurity per-клеточный аккумулятор, не сигнал уровня биом-графа), её
// проявленность проверяется отдельным IsBereginyaManifested()
// (GridWorldManagerArtifacts.cpp), не общим IsLegendaryManifested().
//
// Зеркальце/Гамаюн и Клубочек/Мать-Сыра-Земля (bWarmsCompanionItem) —
// ревизия "Update docs" (2026-09-01, §21.2) сняла их особый статус:
// "Аграфена их не даёт... оба — такие же артефакты, как остальные шесть".
// Получают запись в AGridWorldManager::AcquiredArtifacts на общих
// основаниях (нужна для Warmth/прогрева §21.4). bWarmsCompanionItem
// теперь значит только "вызывающая сторона (HerbalistPlayerController::
// OfferForArtifact) дополнительно выставляет bHasMirror/bHasYarnBall" —
// оба предмета физически другой формы (не инвентарный слот), поэтому им
// ещё и нужен этот второй, контроллерный флаг присутствия.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "ArtifactTypes.generated.h"

USTRUCT()
struct FArtifactDefinition
{
    GENERATED_BODY()

    UPROPERTY() FName ArtifactID;

    // Пусто только для Гребня (Берегиня) — см. комментарий в шапке файла.
    UPROPERTY() FName LegendaryEntityID;

    UPROPERTY() EBiomeType Biome = EBiomeType::ForestSteppe;

    // Только Фонарь — единственный биом (Болото) без благого Легендарного,
    // честного пути к нему нет вовсе (§21.3).
    UPROPERTY() bool bDeceptionOnly = false;

    // Только Фонарь — прогревается от общей GlobalPerceptionClarity, не от
    // локального Restoration/Respect родного региона (§21.4: "Обман не может
    // прогреться через отношения с Болотным царём... сознательная асимметрия,
    // не баг"). Сам активный прогрев-тик — открытый вопрос §21.5, здесь
    // только маркер для будущего прохода.
    UPROPERTY() bool bWarmsFromGlobalClarity = false;

    // См. комментарий в шапке файла.
    UPROPERTY() bool bWarmsCompanionItem = false;
};

// Разовая, честно/обманом добытая запись (§21.3-21.4) — та же логика,
// что уже отличает S_real/S_Perceived в тултипе, применённая к подношению.
// Warmth — структурно готовый аккумулятор прогрева, конкретные пороги
// "апгрейда" НЕ определены (§21.5: "открытые вопросы для следующего
// прохода") — не изобретаю числа, поле стоит на 0 до отдельного решения.
USTRUCT()
struct FAcquiredArtifact
{
    GENERATED_BODY()

    UPROPERTY() FName ArtifactID;
    UPROPERTY() bool bAcquiredViaDeception = false;
    UPROPERTY() float Warmth = 0.0f;

    // Только Камень-оберег (§21.3: "гасит худший исход Bifurcation один
    // раз... затем расходуется до следующего прогрева"). Прогрев (Warmth
    // выше) не тикает активно в этом проходе — на практике "до следующего
    // прогрева" здесь означает "до конца партии", раз recharge не
    // реализован; честно, не выдаю за постоянную защиту.
    UPROPERTY() bool bBifurcationChargeSpent = false;
};

inline const TArray<FArtifactDefinition>& GetArtifactDefinitions()
{
    static const TArray<FArtifactDefinition> Definitions = []()
    {
        TArray<FArtifactDefinition> Defs;

        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Зеркальце"));
            D.LegendaryEntityID = FName(TEXT("Гамаюн"));
            D.Biome = EBiomeType::ForestSteppe;
            D.bWarmsCompanionItem = true;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Клубочек"));
            D.LegendaryEntityID = FName(TEXT("Мать-Сыра-Земля"));
            D.Biome = EBiomeType::Steppe;
            D.bWarmsCompanionItem = true;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Рог"));
            D.LegendaryEntityID = FName(TEXT("Индрик-зверь"));
            D.Biome = EBiomeType::Taiga;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Гребень"));
            D.LegendaryEntityID = NAME_None;   // Берегиня — особый путь, см. шапку файла
            D.Biome = EBiomeType::Floodplain;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Молодильное яблоко"));
            D.LegendaryEntityID = FName(TEXT("Дуб-старец"));
            D.Biome = EBiomeType::BroadleafForest;
            Defs.Add(D);
        }
        {
            // Дубинка/Дубыня изъяты из дизайна этой ревизией (§21.3/§21.5,
            // коммит "Ending and artifacts") — Смешанный лес теперь Баба-Яга.
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Шапка-невидимка"));
            D.LegendaryEntityID = FName(TEXT("Баба-Яга"));
            D.Biome = EBiomeType::MixedForest;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Камень-оберег"));
            D.LegendaryEntityID = FName(TEXT("Волот"));
            D.Biome = EBiomeType::Tundra;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Фонарь"));
            D.LegendaryEntityID = FName(TEXT("Болотный царь"));
            D.Biome = EBiomeType::Bog;
            D.bDeceptionOnly = true;
            D.bWarmsFromGlobalClarity = true;
            Defs.Add(D);
        }

        return Defs;
    }();
    return Definitions;
}

inline const FArtifactDefinition* FindArtifactDefinition(FName ArtifactID)
{
    for (const FArtifactDefinition& D : GetArtifactDefinitions())
    {
        if (D.ArtifactID == ArtifactID) return &D;
    }
    return nullptr;
}
