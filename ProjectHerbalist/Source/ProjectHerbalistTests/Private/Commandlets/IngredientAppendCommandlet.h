// IngredientAppendCommandlet.h
//
// Добавляет строки из herbalist_docs/CSV_tabs/ingredients.json в живой
// DT_IngredientClass, не трогая уже существующие ряды: читает текущее
// состояние таблицы через встроенный UDataTable::GetTableAsJSON() (тот же
// формат, что и штатный экспорт из редактора, включая NSLOCTEXT-обёртку
// FText), добавляет только ряды с именами из --Names=, и записывает всё
// обратно одним UDataTable::CreateTableFromJSONString() — так исходные 71+
// строк не пересобираются вручную и не рискуют потерять Icon/ResourceMesh,
// если те когда-нибудь были проставлены в редакторе.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=IngredientAppend -Names=broad_10,les_09,riv_11,ste_08,tai_10
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "IngredientAppendCommandlet.generated.h"

UCLASS()
class UIngredientAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
