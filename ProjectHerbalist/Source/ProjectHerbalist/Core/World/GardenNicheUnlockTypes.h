// GardenNicheUnlockTypes.h
//
// Экономика пристроек сада (DESIGN_Community_And_Homestead.md §2.4/§2.2,
// "полировка" 2026-09-06, прямой запрос "Делаем сад"). Документ называет
// принцип ("мягкая прокачка: физически есть редкие материалы И достаточное
// отношение — Molva для общинных построек, Respect хозяина для связанных
// с ним конкретно"), но не называет ни одного конкретного рецепта — ни
// здесь, ни у аналогичного апгрейда хранилища/котла (§2.2), который тоже
// остаётся "структурным принципом без конкретного рецепта" (см. ROADMAP.md).
// Решения пользователя 2026-09-06 (три ответа на уточняющие вопросы):
//   1. Постройка РАСХОДУЕТ материал (разовая постройка, не экипировка).
//   2. Порог отношений — Molva для всех пяти обычных ниш (сад стоит у
//      жилища игрока, не привязан к конкретному хозяину места) — КРОМЕ
//      Пещеры: без порога Molva вовсе (личное дело — копаешь грот сам,
//      не общинная постройка), только материал.
//   3. Точные материалы/числа — черновик, предложенный на сверку, принят
//      пользователем как временный ("пока принимаем так, потом изменим") —
//      не измеренные константы, ЧЕРНОВЫЕ по прямому решению, подлежат
//      пересмотру.
//
// Подбор материала на нишу — реальные карточки компендиума, выбранные по
// текстовому/тегово-му соответствию (не архитектурная случайность). ID —
// короткий код строки DT_IngredientClass (ingredients.json::Name), НЕ
// DisplayName -- та же путаница, которую стоит держать в уме на будущее:
// дикорастущие травы компендиума ключуются кодом ("riv_08"), а не русским
// именем (в отличие от Перегноя/крестов оберегов/инструментов/контейнеров,
// добавленных отдельными комментлетами с русским именем ключом напрямую):
//   - Грибница: Перегной ("Перегной") x3 — сама ниша описана как "тёмный
//     ящик на гнилой древесине", Перегной (терминальный продукт гниения,
//     2026-09-04) и есть эта гнилая органика.
//   - Погреб-ледник: Дубовая кора ("broad_10") x3 — тег карточки "оберег,
//     несгибаемость", дуб как самая прочная древесина сруба в реальной
//     традиции.
//   - Водоём: Рогоз ("riv_08") x3 — реальная камышовая выстилка кадки/
//     пруда, карточка прямо несёт тег "рогоз".
//   - Открытая грядка (солнце): Сосна ("tai_03") x3 — самый частый
//     обиходный сруб-материал, самая населённая ниша компендиума
//     (16 карточек) получает самый низкий порог Molva.
//   - Тенистая грядка подлеска: Копытень ("broad_03") x3 — карточка несёт
//     тег "подлесник", буквальное текстовое совпадение с именем ниши.
//   - Пещера: Верес/можжевельник ("tai_05") x5 — плотная, стойкая к гнили
//     древесина реальных мелких поделок; выше количество и без Molva-
//     порога — гейт к артефакт-тиру (кристаллы оберегов), не общинная
//     постройка.
//
// Molva-пороги посчитаны от MolvaOfferingGain=0.03 (HerbalistSettings.h) --
// 0.3 достижимо примерно за 15-20 честных подношений, не мгновенно, тот же
// принцип "отношения со временем", что уже у остальных Molva-порогов
// проекта (TradeMolvaRateBonus и др.).
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "GardenNicheUnlockTypes.generated.h"

USTRUCT()
struct FGardenNicheUnlockCost
{
    GENERATED_BODY()

    UPROPERTY() EGardenNiche Niche = EGardenNiche::None;
    UPROPERTY() FName MaterialIngredientID = NAME_None;
    UPROPERTY() int32 MaterialCount = 0;

    // Диапазон Molva -- [-1, 1] (GridWorldManagerCommunity.cpp). Сентинел
    // -1.0f значит "порог отношений не требуется вовсе" (Пещера) -- Molva
    // всегда >= -1.0f по построению (FMath::Clamp), проверка тривиально
    // проходит без отдельного bool-флага "RequiresMolva".
    UPROPERTY() float MinMolva = -1.0f;
};

// Сознательно небольшой статический список, не DataTable-пайплайн -- тот
// же принцип, что уже RitualTypes.h::GetRitualRecipeDefinitions (шесть
// записей, редко меняются, все черновые числа читаются построчно из
// комментария в шапке файла).
inline const TArray<FGardenNicheUnlockCost>& GetGardenNicheUnlockCosts()
{
    static const TArray<FGardenNicheUnlockCost> Costs = []()
    {
        TArray<FGardenNicheUnlockCost> C;

        FGardenNicheUnlockCost Mycelium;
        Mycelium.Niche = EGardenNiche::Mycelium;
        Mycelium.MaterialIngredientID = FName(TEXT("Перегной"));
        Mycelium.MaterialCount = 3;
        Mycelium.MinMolva = 0.3f;
        C.Add(Mycelium);

        FGardenNicheUnlockCost RootCellar;
        RootCellar.Niche = EGardenNiche::RootCellar;
        RootCellar.MaterialIngredientID = FName(TEXT("broad_10"));   // Дубовая кора
        RootCellar.MaterialCount = 3;
        RootCellar.MinMolva = 0.3f;
        C.Add(RootCellar);

        FGardenNicheUnlockCost Pond;
        Pond.Niche = EGardenNiche::Pond;
        Pond.MaterialIngredientID = FName(TEXT("riv_08"));   // Рогоз (Початки, Батлачок)
        Pond.MaterialCount = 3;
        Pond.MinMolva = 0.3f;
        C.Add(Pond);

        FGardenNicheUnlockCost SunnyBed;
        SunnyBed.Niche = EGardenNiche::SunnyBed;
        SunnyBed.MaterialIngredientID = FName(TEXT("tai_03"));   // Сосна (Перунова сестра)
        SunnyBed.MaterialCount = 3;
        SunnyBed.MinMolva = 0.2f;
        C.Add(SunnyBed);

        FGardenNicheUnlockCost ShadeBed;
        ShadeBed.Niche = EGardenNiche::ShadeBed;
        ShadeBed.MaterialIngredientID = FName(TEXT("broad_03"));   // Копытень (Копыто, Копычник, Рвотный корень)
        ShadeBed.MaterialCount = 3;
        ShadeBed.MinMolva = 0.2f;
        C.Add(ShadeBed);

        FGardenNicheUnlockCost Cave;
        Cave.Niche = EGardenNiche::Cave;
        Cave.MaterialIngredientID = FName(TEXT("tai_05"));   // Верес (Можжевельник, Яловець)
        Cave.MaterialCount = 5;
        Cave.MinMolva = -1.0f;   // без порога Molva -- личная постройка, не общинная
        C.Add(Cave);

        return C;
    }();
    return Costs;
}

inline const FGardenNicheUnlockCost* FindGardenNicheUnlockCost(EGardenNiche Niche)
{
    for (const FGardenNicheUnlockCost& Cost : GetGardenNicheUnlockCosts())
    {
        if (Cost.Niche == Niche) return &Cost;
    }
    return nullptr;
}
