#include "IngredientRegistry.h"

TMap<FName, EIngredientClass> FIngredientRegistry::Registry;

void FIngredientRegistry::Initialize()
{
    Registry.Empty();

    // Вода
    Registry.Add(FName(TEXT("Water")), EIngredientClass::Water);

    // Болотные травы
    Registry.Add(FName(TEXT("bol_01")), EIngredientClass::Herb);   // Багульник
    Registry.Add(FName(TEXT("bol_02")), EIngredientClass::Herb);   // Белокрыльник
    Registry.Add(FName(TEXT("bol_04")), EIngredientClass::Herb);   // Трифоль (Вахта)
    Registry.Add(FName(TEXT("bol_08")), EIngredientClass::Herb);   // Резун (Осока)
    Registry.Add(FName(TEXT("bol_09")), EIngredientClass::Herb);   // Пухлянка (Пушица)
    Registry.Add(FName(TEXT("bol_10")), EIngredientClass::Herb);   // Царевы очи (Росянка)
    Registry.Add(FName(TEXT("bol_11")), EIngredientClass::Herb);   // Девятисил (Сабельник)

    // Болотные ягоды
    Registry.Add(FName(TEXT("bol_05")), EIngredientClass::Berry);  // Гонобобель (Голубика)
    Registry.Add(FName(TEXT("bol_06")), EIngredientClass::Berry);  // Журавина (Клюква)

    // Болотные мхи
    Registry.Add(FName(TEXT("bol_12")), EIngredientClass::Moss);   // Бѣлый мохъ (Сфагновые мхи)

    // Болотные деревья
    Registry.Add(FName(TEXT("bol_03")), EIngredientClass::Wood);   // Берёза пушистая
    Registry.Add(FName(TEXT("bol_07")), EIngredientClass::Wood);   // Елха чёрная (Ольха чёрная)
}

EIngredientClass FIngredientRegistry::GetClass(FName IngredientID)
{
    if (const EIngredientClass* Found = Registry.Find(IngredientID))
        return *Found;
    return EIngredientClass::Unknown;
}