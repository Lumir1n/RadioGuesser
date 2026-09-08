// Copyright RadioGuesser. All Rights Reserved.

#include "Game/RGGameState.h"
#include "Net/UnrealNetwork.h"

void ARGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ARGGameState, GamePhase);
    DOREPLIFETIME(ARGGameState, CurrentRound);
    DOREPLIFETIME(ARGGameState, TotalRounds);
    DOREPLIFETIME(ARGGameState, RoundTimeRemaining);
    DOREPLIFETIME(ARGGameState, RoundToken);
    DOREPLIFETIME(ARGGameState, StationDisplayName);
    DOREPLIFETIME(ARGGameState, StreamUrl);
}
