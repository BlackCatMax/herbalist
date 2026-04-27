// ItemTooltipWidget.cpp
#include "UI/ItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Pipeline/Perception.h"
#include "Core/Pipeline/AlchemySemantics.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Player/HerbalistPlayerController.h"

FText GeneratePotionName(const FRealState& State)
{
    struct FAxisInfo { FString Name; float Value; };
    FAxisInfo Axes[] = {
        {TEXT("Телесное"), State.Direction.Body},
        {TEXT("Разумное"), State.Direction.Mind},
        {TEXT("Духовное"), State.Direction.Spirit},
        {TEXT("Природное"), State.Direction.Nature}
    };
    int Best = 0;
    for (int i = 1; i < 4; ++i)
        if (Axes[i].Value > Axes[Best].Value) Best = i;
    
    FString MainAxis = Axes[Best].Name;
    float Distortion = State.Meta.Distortion;
    float Purity = State.Meta.Purity;
    
    FString Quality;
    if (Distortion < 0.3f)
        Quality = TEXT("Чистое");
    else if (Distortion < 0.6f)
        Quality = TEXT("Мутное");
    else
        Quality = TEXT("Искажённое");
    
    if (Distortion < 0.5f && Purity > 0.5f && State.Meta.Stability > 0.5f)
        Quality = TEXT("Очищенное");
    
    return FText::FromString(FString::Printf(TEXT("%s %s зелье"), *Quality, *MainAxis));
}

void UItemTooltipWidget::SetItem(const FInventoryItem& Item, float GlobalDistortion)
{
    if (Item.IsEmpty()) return;

    FRandomStream Random(GetTypeHash(Item.IngredientID) ^ static_cast<int32>(GlobalDistortion * 1000.0f));

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwningPlayer());
    UIngredientRegistrySubsystem* IngredientSubsystem = nullptr;
    if (PC)
    {
        UGameInstance* GameInstance = PC->GetGameInstance();
        IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    }

    FString Name;
    if (Item.IngredientID == FName(TEXT("Potion")))
    {
        Name = GeneratePotionName(Item.State).ToString();
    }
    else if (Item.IngredientID == FName(TEXT("Ash")))
    {
        Name = TEXT("Зола");
    }
    else if (Item.IngredientID == FName(TEXT("BoiledWater")))
    {
        Name = TEXT("Кипячёная вода");
    }
    else if (Item.IngredientID == FName(TEXT("Water")))
    {
        Name = TEXT("Вода");
    }
    else
    {
        EIngredientClass RealClass = IngredientSubsystem ? IngredientSubsystem->Classify(Item.IngredientID) : EIngredientClass::Unknown;
        EIngredientClass PerceivedClass = Perception::PerceiveClass(RealClass, GlobalDistortion, Random);
        
        if (PerceivedClass != RealClass)
        {
            Name = FString::Printf(TEXT("%s?"), *Item.IngredientID.ToString());
        }
        else
        {
            Name = Item.IngredientID.ToString();
        }
    }

    if (NameText) NameText->SetText(FText::FromString(Name));

    EIngredientClass IngClass = IngredientSubsystem ? IngredientSubsystem->Classify(Item.IngredientID) : EIngredientClass::Unknown;
    FString ClassName;
    switch (IngClass)
    {
    case EIngredientClass::Water:    ClassName = TEXT("Вода"); break;
    case EIngredientClass::Plant:    ClassName = TEXT("Растение"); break;
    case EIngredientClass::Mineral:  ClassName = TEXT("Минерал"); break;
    case EIngredientClass::Fungus:   ClassName = TEXT("Гриб"); break;
    case EIngredientClass::Catalyst: ClassName = TEXT("Катализатор"); break;
    case EIngredientClass::Essence:  ClassName = TEXT("Эссенция"); break;
    default:                         ClassName = TEXT("Неизвестное"); break;
    }
    if (TypeText) TypeText->SetText(FText::FromString(ClassName));

    auto SetDistortedText = [&](UTextBlock* TextBlock, float RealValue, const FString& Label)
    {
        if (!TextBlock) return;
        float Perceived = Perception::PerceiveValue(RealValue, GlobalDistortion, Random);
        TextBlock->SetText(FText::FromString(FString::Printf(TEXT("%s: %.3f (%.3f)"), *Label, Perceived, RealValue)));
    };

    SetDistortedText(MagnitudeText,        Item.State.Magnitude,          TEXT("Сила"));
    SetDistortedText(DistortionText,       Item.State.Meta.Distortion,    TEXT("Искажение"));
    SetDistortedText(PurityText,           Item.State.Meta.Purity,        TEXT("Чистота"));
    SetDistortedText(StabilityText,        Item.State.Meta.Stability,     TEXT("Стабильность"));
    SetDistortedText(PotencyText,          Item.State.Meta.Potency,       TEXT("Мощность"));
    SetDistortedText(ResonanceText,        Item.State.Meta.Resonance,     TEXT("Резонанс"));
    SetDistortedText(CorruptionText,       Item.State.Meta.Corruption,    TEXT("Порча"));

    SetDistortedText(DirectionBodyText,    Item.State.Direction.Body,     TEXT("Тело"));
    SetDistortedText(DirectionMindText,    Item.State.Direction.Mind,     TEXT("Разум"));
    SetDistortedText(DirectionSpiritText,  Item.State.Direction.Spirit,   TEXT("Дух"));
    SetDistortedText(DirectionNatureText,  Item.State.Direction.Nature,   TEXT("Природа"));
}