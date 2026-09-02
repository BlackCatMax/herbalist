// ArtifactTypes.h
//
// Артефакты Легендарных сущностей (21_Journey_And_Artifacts.md §21.3-21.4,
// 2026-09-01). Тот же паттерн реестра-DataTable, что LegendaryEntityTypes.h/
// LandmarkTypes.h/AmbientEntityTypes.h (все четверо мигрировали 2026-09-02,
// см. ниже).
//
// Восемь строк ровно по таблице §21.3. Гребень → LegendaryEntityID
// "Берегиня" — 2026-09-02, унификация Берегини: она теперь обычная строка
// LegendaryEntityTypes.h (bUsesCellHistoryPurity=true), больше не особый
// случай без ID; IsLegendaryManifested сама умеет её проверить (fallback-
// скан без якоря), отдельного IsBereginyaManifested() не осталось.
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
//
// 2026-09-02, Unit 4/6 миграции контента проекта на DataTable — тот же
// паттерн, что уже AmbientEntityTypes.h/LandmarkTypes.h/LegendaryEntityTypes.h
// (юниты 1-3, бестиарий): GetArtifactDefinitions() ниже лениво грузит
// /Game/Herbalist/Data/DT_Artifacts. FAcquiredArtifact НЕ мигрирует — это
// не карточка-определение, а runtime-запись (AGridWorldManager::
// AcquiredArtifacts), тот же класс, что FGridCell — состояние, не контент.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Engine/DataTable.h"
#include "ArtifactTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FArtifactDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY() FName ArtifactID;

    // Явный порядок регистрации — тот же приём, что у остальных пяти
    // мигрированных реестров (см. FAmbientEntityDefinition::SortOrder).
    UPROPERTY() int32 SortOrder = 0;

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

// Ленивая загрузка из /Game/Herbalist/Data/DT_Artifacts (2026-09-02) —
// тот же паттерн, что GetLegendaryEntityDefinitions() и др., см. подробное
// обоснование в AmbientEntityTypes.h. LogTemp, не HerbalistLogChannels.h
// категория — та же причина (LNK2001): inline-функция компилируется и в
// ProjectHerbalistTests через новый коммандлет.
inline const TArray<FArtifactDefinition>& GetArtifactDefinitions()
{
    static const TArray<FArtifactDefinition> Definitions = []()
    {
        check(IsInGameThread());   // LoadObject не потокобезопасен

        TArray<FArtifactDefinition> Defs;
        UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_Artifacts"));
        if (!Table)
        {
            UE_LOG(LogTemp, Error, TEXT("GetArtifactDefinitions: не удалось загрузить DT_Artifacts -- реестр артефактов будет пуст"));
            return Defs;
        }
        Table->AddToRoot();

        TArray<FArtifactDefinition*> Rows;
        Table->GetAllRows(TEXT("GetArtifactDefinitions"), Rows);
        Defs.Reserve(Rows.Num());
        for (const FArtifactDefinition* Row : Rows)
        {
            if (Row) Defs.Add(*Row);
        }
        Defs.Sort([](const FArtifactDefinition& A, const FArtifactDefinition& B) { return A.SortOrder < B.SortOrder; });
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
