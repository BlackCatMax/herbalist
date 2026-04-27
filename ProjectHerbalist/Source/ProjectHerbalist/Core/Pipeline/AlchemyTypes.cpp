// AlchemyTypes.cpp
#include "AlchemyTypes.h"
#include "ProjectHerbalist.h"

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
                           EIngredientClass InClass, EAtomOrigin InOrigin,
                           float InDistortionAtCollection, float InTimeOfCreation)
    : bIsWater(bInIsWater)
    , SourceID(InSourceID)
    , State(InState)
    , AtomUID(FGuid::NewGuid())
    , Class(InClass)
    , OriginContext(InOrigin)
    , DistortionAtCollection(InDistortionAtCollection)
    , TimeOfCreation(InTimeOfCreation)
{
    if (Class == EIngredientClass::Unknown)
    {
        UE_LOG(LogHerbalist, Verbose, TEXT("[Herbalist] FAlchemyAtom created with Unknown class: '%s' (UID: %s)"),
            *InSourceID.ToString(), *AtomUID.ToString());
    }
}