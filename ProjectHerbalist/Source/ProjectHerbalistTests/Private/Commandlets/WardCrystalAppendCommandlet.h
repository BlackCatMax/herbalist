// WardCrystalAppendCommandlet.h
//
// Добавляет ряды для кристаллов Пещеры (DESIGN_Community_And_
// Homestead.md §2.4, механика оберегов, 2026-09-04 первый заход + второй
// заход того же дня -- Куриный бог/MorokReduction) в живой DT_IngredientClass
// -- тот же UDataTable::AddRow-приём, что уже ArtifactIngredientAppendCommandlet
// применяет к артефактам/перьям: строится программно в C++, не из JSON --
// эти 2 строки не входят ни в компендиум трав (herbalist_docs/CSV_tabs/
// ingredients.json), ни в проход по 76 карточкам растений, это отдельная,
// новая категория (минералы), с собственными компендиумными карточками
// (herbalist_docs/Herbalist_Vault/04_Compendium/Минералы/).
//
// AllowedBiomes пуст у обоих -- та же семантика, что уже устоялась
// ("пустой список значит нигде не растёт"): кристалл никогда не выпадает
// случайным сбором, только через нишу сада (GardenNiche::Cave). DecayRate=0
// -- минерал физически не портится, тот же довод, что артефакты (не игровой
// баланс, факт материала). Resilience=0 (НЕ 1.0, как у Дубовой коры) --
// намеренно: сад должен уметь тянуть Cell.State к BaseState кристалла
// (DESIGN_Community_And_Homestead.md §2.4: "кристаллы растут тем же жестом
// State->BaseState, что и травы"), Resilience=1.0 сломал бы этот жест точно
// так же, как сломал его у Дубовой коры.
//
// Идемпотентен: ряд, уже присутствующий в таблице (найден по имени),
// пропускается с предупреждением, не перезаписывается.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=WardCrystalAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WardCrystalAppendCommandlet.generated.h"

UCLASS()
class UWardCrystalAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
