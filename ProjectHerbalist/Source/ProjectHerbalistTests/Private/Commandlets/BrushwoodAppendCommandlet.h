// BrushwoodAppendCommandlet.h
//
// Добавляет ряд «Хворост» в живой DT_IngredientClass (диегетический
// интерфейс, этап 6: лагерь в пути тратит одну вязанку; решения пользователя
// 2026-09-21 -- «отдельный ресурс в лесу», «одна вязанка»). Числа -- из
// карточки компендиума (04_Compendium/Растительность/Смешанный лес/Хворост.md)
// и её отражения в CSV_tabs/ingredients.json.
//
// Точечный UDataTable::AddRow, как ContainerAppend/PeregnoyAppend -- НЕ
// IngredientAppend: тот проходит всю таблицу через JSON и переименовывал ряды
// с пробелом в имени («Молодильное яблоко», «Железный серп», найдено здесь же
// 2026-09-21, таблица возвращена из git).
//
// Идемпотентен: ряд уже есть -- пропуск с предупреждением.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=BrushwoodAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BrushwoodAppendCommandlet.generated.h"

UCLASS()
class UBrushwoodAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
