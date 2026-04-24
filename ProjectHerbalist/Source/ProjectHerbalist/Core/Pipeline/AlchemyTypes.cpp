#include "AlchemyTypes.h"
#include "Core/Data/IngredientRegistry.h"

FAlchemyAtom::FAlchemyAtom()
    : bIsWater(false)
    , SourceID(NAME_None)
    , State(FRealState())
    , AtomUID(FGuid::NewGuid())
    , Class(EIngredientClass::Unknown)
    , OriginContext(EAtomOrigin::Unknown)
    , DistortionAtCollection(0.3f)
    , TimeOfCreation(0.0f)
{
}

FAlchemyAtom::FAlchemyAtom(FName InSourceID, bool bInIsWater, const FRealState& InState,
                           EAtomOrigin InOrigin, float InDistortionAtCollection, float InTimeOfCreation)
    : bIsWater(bInIsWater)
    , SourceID(InSourceID)
    , State(InState)
    , AtomUID(FGuid::NewGuid())
    , Class(FIngredientRegistry::Classify(InSourceID))
    , OriginContext(InOrigin)
    , DistortionAtCollection(InDistortionAtCollection)
    , TimeOfCreation(InTimeOfCreation)
{
    if (Class == EIngredientClass::Unknown)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Herbalist] FAlchemyAtom created with Unknown class: '%s' (UID: %s)"),
            *InSourceID.ToString(), *AtomUID.ToString());
    }
}