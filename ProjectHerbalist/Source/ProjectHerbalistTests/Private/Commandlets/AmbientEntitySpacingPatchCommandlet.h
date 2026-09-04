// AmbientEntitySpacingPatchCommandlet.h
//
// Проставляет FAmbientEntityDefinition::MinSpacingMeters (2026-09-03,
// AmbientEntityTypes.h) на уже существующих 33 рядах DT_AmbientEntities --
// прямой запрос пользователя 2026-09-04 ("проставь Min Spacing по видам").
// Точечная правка через Table->FindRow (не полный JSON-роундтрип таблицы --
// тот же урок, что и IngredientGatheringAndGardenPatchCommandlet/
// IngredientHarvestWindowPatchCommandlet, 2026-09-04: GetTableAsJSON/
// CreateTableFromJSONString молча терял ряды с пробелом в имени, а у
// бестиария пробел в имени -- норма ("Курганные огни", "Ржавые духи"), не
// исключение, так что здесь такой приём был бы ещё опаснее).
//
// Источник данных -- herbalist_docs/CSV_tabs/ambient_entity_spacing.json.
// Числа НЕ индивидуальны на все 33 вида -- прямых текстовых улик о
// количестве в карточках компендиума почти нет (проверено построчно), три
// группы по фактической текстовой опоре: 0 м -- явный рой/стая ("нападают
// стаей/стайками", Гнильники/Суховейки); 30 м -- явная территориальность,
// карточка привязывает вид к одному кургану/меже и держит "охраняет свою
// территорию" (Курганники/Курганные огни/Чащобные духи/Жердяи); 15 м --
// все остальные 27, для которых карточка не даёт ни роя, ни территории --
// не 27 придуманных чисел, а честная средняя ставка вместо неизвестности.
// См. точную разбивку по видам и цитаты -- CHANGELOG.md.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=AmbientEntitySpacingPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AmbientEntitySpacingPatchCommandlet.generated.h"

UCLASS()
class UAmbientEntitySpacingPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
