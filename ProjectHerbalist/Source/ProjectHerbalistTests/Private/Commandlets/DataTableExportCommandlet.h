// DataTableExportCommandlet.h
//
// Выгружает НАСТОЯЩЕЕ содержимое игровых DataTable в json-файлы, чтобы его
// можно было механически сверить с документацией и с экспортами в
// `herbalist_docs/CSV_tabs/` (2026-09-07, запрос пользователя "проверить
// остальные json" после того, как у `DT_BiomeDefaults` нашлось расхождение
// ассета с компендиумом в 35 значениях из 112).
//
// Зачем отдельный инструмент: содержимое `.uasset` — двоичное, и до сих пор
// единственным способом узнать, что там на самом деле лежит, было открыть
// редактор или напечатать значения из теста. Оба способа не годятся для
// сверки целых таблиц на сотню строк.
//
// Пишет в папку, указанную `-out=`, по файлу на таблицу
// (`<ИмяТаблицы>.actual.json`). Ничего не меняет в проекте — только читает.
// Именно `.actual.json`, а не поверх файлов в `CSV_tabs/`: те считаются
// отражением ДОКУМЕНТАЦИИ, и затирать их содержимым ассета значило бы
// потерять ровно ту разницу, ради которой всё и затевалось.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=DataTableExport -out=C:\путь
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DataTableExportCommandlet.generated.h"

UCLASS()
class UDataTableExportCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
