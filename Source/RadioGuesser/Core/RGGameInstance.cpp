// Copyright RadioGuesser. All Rights Reserved.

#include "Core/RGGameInstance.h"
#include "RadioGuesser.h"

void URGGameInstance::Init()
{
    Super::Init();
    UE_LOG(LogMatch, Log, TEXT("RadioGuesser GameInstance initialised"));
}

void URGGameInstance::Shutdown()
{
    UE_LOG(LogMatch, Log, TEXT("RadioGuesser GameInstance shutting down"));
    Super::Shutdown();
}
