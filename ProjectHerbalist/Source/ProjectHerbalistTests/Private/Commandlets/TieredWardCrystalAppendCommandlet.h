// TieredWardCrystalAppendCommandlet.h
//
// Добавляет ряды для ТРЁХ тиражных кристаллов-оберегов (награда ритуалов
// перехода ярусов биомов, DESIGN_Community_And_Homestead.md §2.4/ROADMAP.md
// "рецептурный гейт между ярусами биомов", 2026-09-04) в живой
// DT_IngredientClass -- тот же UDataTable::AddRow-приём, что уже
// WardCrystalAppendCommandlet применяет к трём исходным кристаллам Пещеры
// (Плакун-камень/Громовая стрела/Куриный бог): строится программно в C++,
// не из JSON -- эти 3 строки, как и те, не входят ни в компендиум трав
// (herbalist_docs/CSV_tabs/ingredients.json), ни в проход по карточкам
// растений, отдельная категория минералов, свои компендиумные карточки
// (herbalist_docs/Herbalist_Vault/04_Compendium/Минералы/).
//
// В ОТЛИЧИЕ от трёх исходных -- GardenNiche::None, не Cave: эти три
// кристалла НЕ растут в Пещере и вообще нигде в мире (AllowedBiomes тоже
// пуст) -- единственный путь получения -- завершить соответствующий ритуал
// (RitualTypes.h::FRitualRecipeDefinition::GrantsIngredientID,
// TryAdvanceRitual). Resilience=1.0 (не 0.0, как у исходных трёх) --
// намеренно другое решение: исходным Resilience=0.0 нужен, чтобы сад умел
// тянуть Cell.State к BaseState (GardenNiche::Cave, жест State->BaseState);
// этим трём кристаллам сад вообще не важен, они никогда не занимают клетку
// сада -- Resilience=1.0 (та же логика, что у Дубовой коры) честнее
// отражает "эти кристаллы никогда не меняются местом", раз механизм,
// оправдывавший 0.0 у оригиналов, здесь просто не применяется.
//
// bIsTieredWard=true + WardHomeBiomes (IngredientTableRow.h, 2026-09-04) --
// у трёх исходных кристаллов эти поля НЕ трогаются (остаются false/пусто).
//
// Идемпотентен: ряд, уже присутствующий в таблице (найден по имени),
// пропускается с предупреждением, не перезаписывается.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=TieredWardCrystalAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "TieredWardCrystalAppendCommandlet.generated.h"

UCLASS()
class UTieredWardCrystalAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
