// Source/ProjectHerbalistTests/Private/Tests/BestiaryRankConsistencyTest.cpp
//
// Ранг существа не должен расходиться между компендиумом и реестрами
// (2026-09-03). Сверка -run=CompendiumAudit находит такие расхождения, но
// она ручная и читает карточки с диска — здесь тот же инвариант закреплён
// автотестом на самих реестрах, чтобы регенерация таблиц или правка
// генератора не вернула расхождение молча.
//
// Проверяется ДВА разных инварианта, и второй важнее первого:
//
//   1. Три конкретных существа (Курганники, Жердяи, Курганные огни) живут
//      в Низшем ранге и НЕ в Основном — то решение, ради которого их
//      переносили. Именованная проверка: если кто-то вернёт строки в
//      DT_Landmarks, тест назовёт виновника.
//
//   2. НИ ОДНО существо не числится в двух рангах одновременно. Это общий
//      закон, а не три имени: ровно так выглядел бы дубль, если бы перенос
//      сделали добавлением без удаления (а именно этой ошибки я и опасался,
//      когда отказался генерировать по первому отчёту заготовки).
//
// Реестры читаются напрямую (GetAmbientEntityDefinitions/
// GetLandmarkDefinitions/GetLegendaryEntityDefinitions) — они
// самоинициализирующиеся, мир и BeginPlay для этого не нужны, поэтому тест
// не поднимает AGridWorldManager вовсе.

#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Entities/LandmarkTypes.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBestiary_KurganSpiritsAreLowRank,
    "Herbalist.Bestiary.KurganSpiritsAreLowRank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBestiary_KurganSpiritsAreLowRank::RunTest(const FString& Parameters)
{
    const TArray<FName> Moved = {
        FName(TEXT("Курганники")),
        FName(TEXT("Жердяи")),
        FName(TEXT("Курганные огни")),
    };

    TSet<FName> InAmbient;
    for (const FAmbientEntityDefinition& Def : GetAmbientEntityDefinitions())
    {
        InAmbient.Add(Def.EntityID);
    }

    TSet<FName> InLandmarks;
    for (const FLandmarkDefinition& Def : GetLandmarkDefinitions())
    {
        InLandmarks.Add(Def.EntityID);
    }

    for (const FName& Id : Moved)
    {
        TestTrue(FString::Printf(TEXT("%s зарегистрирован Низшим рангом"), *Id.ToString()),
            InAmbient.Contains(Id));
        TestFalse(FString::Printf(TEXT("%s больше не числится Основным рангом"), *Id.ToString()),
            InLandmarks.Contains(Id));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBestiary_NoEntityIsRegisteredInTwoRanks,
    "Herbalist.Bestiary.NoEntityIsRegisteredInTwoRanks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBestiary_NoEntityIsRegisteredInTwoRanks::RunTest(const FString& Parameters)
{
    // Имя -> в скольких рангах встретилось, и в каких именно (для внятного
    // сообщения об ошибке: «Х в двух рангах» без названий рангов заставило
    // бы искать руками).
    TMap<FName, TArray<FString>> RanksByEntity;

    for (const FAmbientEntityDefinition& Def : GetAmbientEntityDefinitions())
    {
        RanksByEntity.FindOrAdd(Def.EntityID).Add(TEXT("Низший"));
    }
    for (const FLandmarkDefinition& Def : GetLandmarkDefinitions())
    {
        RanksByEntity.FindOrAdd(Def.EntityID).Add(TEXT("Основной"));
    }
    for (const FLegendaryEntityDefinition& Def : GetLegendaryEntityDefinitions())
    {
        RanksByEntity.FindOrAdd(Def.EntityID).Add(TEXT("Легендарный"));
    }

    bool bAllSingleRank = true;
    for (const TPair<FName, TArray<FString>>& Pair : RanksByEntity)
    {
        if (Pair.Value.Num() > 1)
        {
            bAllSingleRank = false;
            AddError(FString::Printf(TEXT("%s числится сразу в рангах: %s"),
                *Pair.Key.ToString(), *FString::Join(Pair.Value, TEXT(", "))));
        }
    }

    TestTrue(TEXT("Каждое существо принадлежит ровно одному рангу"), bAllSingleRank);

    // Страховка от «тест зелёный, потому что реестры пусты»: если бы
    // загрузка таблиц отвалилась, цикл выше не нашёл бы ни одного дубля и
    // молча отчитался бы об успехе.
    TestTrue(TEXT("Реестры вообще загрузились"), RanksByEntity.Num() > 20);

    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
