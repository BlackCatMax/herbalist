#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HerbalistNameUtils.generated.h"

class UIngredientRegistrySubsystem;
enum class EProcessingStationType : uint8;

// Падеж для склонения фольклорных имён зелий (GeneratePotionName ниже).
// Проекту нужен только именительный сейчас (подписи в UI-слотах), остальные
// падежи — задел на будущие фразы, где имя зелья вклеивается в предложение
// ("благодарит тебя за <Творительный>", "просит принести <Винительный>").
UENUM(BlueprintType)
enum class EGrammaticalCase : uint8
{
    Nominative,     // Именительный — что? зелье
    Genitive,       // Родительный — чего? зелья
    Dative,         // Дательный — чему? зелью
    Accusative,     // Винительный — что? зелье
    Instrumental,   // Творительный — чем? зельем
    Prepositional   // Предложный — о чём? о зелье
};

// Фольклорное имя зелья (02_GDD, находка сессии 2026-08-30: "не топорное
// 'Сильное зелье здоровья', а явно фольклорное и с нормальными падежами").
// Читает EAlchemyOutcome (FInventoryItem::BrewOutcome — честный факт события,
// не искажается восприятием) и State (должен быть ПЕРЕЖДЁННЫЙ/perceived —
// вызывающая сторона решает, реальный или искажённый, см. предупреждение в
// JournalTypes.h про PerceivedState). Полная документация словаря и таблиц
// склонения — в HerbalistNameUtils.cpp.
PROJECTHERBALIST_API FText GeneratePotionName(EAlchemyOutcome Outcome, const FRealState& State,
    EGrammaticalCase Case = EGrammaticalCase::Nominative);

// Единая точка отображаемого имени предмета инвентаря — раньше каждый виджет
// (AlchemySlotWidget/InventorySlotWidget) разрешал Ash/BoiledWater/Water/Potion
// по-своему; один знал ветку "Ash" -> "Зола", другой нет, и то же самое
// IngredientID показывалось по-разному в двух местах интерфейса
// (AUDIT_AND_REFACTORING_PLAN §2.4). Registry может быть nullptr — тогда для
// незнакомых ID возвращается сырой IngredientID.ToString(), как и раньше.
// Item.State используется КАК ЕСТЬ для имени — если вызывающей стороне нужно
// искажённое восприятием имя (обычно нужно, см. AlchemySlotWidget/
// InventorySlotWidget), она обязана заранее подставить в Item.State
// перцептивно искажённую копию, эта функция сама ничего не искажает.
PROJECTHERBALIST_API FString GetItemDisplayName(const FInventoryItem& Item, UIngredientRegistrySubsystem* Registry,
    EGrammaticalCase Case = EGrammaticalCase::Nominative);

// Состояние процессов станций на предмете для подсказки (2026-09-14): сушка,
// отстой, выпаривание -- идёт, закончен или прерван, сколько осталось. Раньше
// игрок никак не видел ни хода, ни итога. HostStation -- станция, в инвентаре
// которой предмет лежит сейчас (None -- сумка или обычное хранилище): таймер,
// начатый в другой станции, стоит, и об этом надо сказать. Флаги и таймеры
// восприятие не искажает -- годится и настоящий, и искажённый предмет.
// Пустая строка -- процессов на предмете не было.
PROJECTHERBALIST_API FString GetItemProcessStatus(const FInventoryItem& Item, EProcessingStationType HostStation);
