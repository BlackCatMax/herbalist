// OrdersCreateCommandlet.h
//
// Каталог заказов /Game/Herbalist/Data/DT_Orders (2026-09-19, 02_GDD/
// 24_Orders_And_Repute.md §24.4): одиннадцать заказов трёх кругов гостей --
// записка словами просителя, область в осях зелья, срок, задаток, плата,
// прибавка за «точно», последствие тёмного заказа и слух о нём. Числа --
// черновик главы.
//
// Ассета нет -- создаёт; есть -- без -sync ничего не делает, с -sync
// переписывает ряды по каталогу ниже (правок в редакторе у заказов пока нет).
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=OrdersCreate [-sync]
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Core/Community/OrderTypes.h"
#include "OrdersCreateCommandlet.generated.h"

UCLASS()
class UOrdersCreateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;

    // Каталог (тест -- без ассета).
    static TArray<FOrderDefinition> BuildOrderRows();
};
