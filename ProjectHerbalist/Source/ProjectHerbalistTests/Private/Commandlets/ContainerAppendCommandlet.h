// ContainerAppendCommandlet.h
//
// Добавляет ряды для ТРЁХ переносных контейнеров игрока (Корзина/Мешок/
// Туёс, DESIGN_Community_And_Homestead.md, "разберём тщательно" систему
// хранения, прямой запрос пользователя 2026-09-04) в живой DT_IngredientClass --
// тот же безопасный точечный UDataTable::AddRow-приём, что уже
// PeregnoyAppendCommandlet/TieredWardCrystalAppendCommandlet (НИКОГДА
// GetTableAsJSON()/CreateTableFromJSONString()).
//
// Все три -- утварь, не растение/минерал/вода: AllowedBiomes пуст (не
// собираются в мире), GardenNiche::None (сад их не касается), DecayRate=0.0
// (сам контейнер не портится), Resilience=1.0 (тот же приём, что уже у
// Перегноя/тиражных оберегов -- сад никогда не тянет их к BaseState места).
// Class=Catalyst -- не расходуемый инструмент/утварь, не сырьё для варки
// (см. довод у EIngredientClass в IngredientTableRow.h).
//
// Экипируются AHerbalistPlayerController::EquipContainer, переключают
// EStorageContainerType личного инвентаря игрока через
// FIngredientTableRow::GrantsContainerType (IngredientTableRow.h) --
// Корзина/Мешок/Туёс дают Basket/Sack/Tues соответственно. НЕ расходуются
// экипировкой (тот же принцип, что уже у активации оберегов).
//
// RarityWeight -- курс общинной торговли (ComputeCommunityTradeValue,
// GridWorldManagerCommunity.cpp) обратно пропорционален полю: "меньше вес
// спавна = реже = ценнее" (уже существующий довод у TradeValueRarityWeight,
// HerbalistSettings.h). Стартовая Корзина -- самая частая и самая дешёвая
// при обмене, поэтому несёт САМЫЙ ВЫСОКИЙ RarityWeight среди трёх (5); Мешок
// чуть реже/дороже (3); Туёс -- качественная общинная награда, самый редкий
// и самый дорогой при обмене, поэтому несёт RarityWeight=1 (тот же пол, что
// у большинства редких карточек компендиума). Это ЧИСЛЕННО обратный порядок
// тому, что подсказывает само название поля -- намеренно, см. формулу.
//
// Идемпотентен: ряд, уже присутствующий в таблице (найден по имени),
// пропускается с предупреждением, не перезаписывается.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=ContainerAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ContainerAppendCommandlet.generated.h"

UCLASS()
class UContainerAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
