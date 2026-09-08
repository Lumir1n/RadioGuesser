// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGPlayerController.h"
#include "RadioGuesser.h"
#include "Map/RGMapSubsystem.h"
#include "Radio/RGRadioSubsystem.h"
#include "Engine/GameInstance.h"

ARGPlayerController::ARGPlayerController()
{
    bShowMouseCursor    = true;
    bEnableClickEvents  = true;
    bEnableMouseOverEvents = true;
}

void ARGPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bGuessSubmitted = false;
}

void ARGPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    // Enhanced Input bindings will be added here in Phase 4
    // when the full input mapping context is set up.
}

// ─── Guess flow ───────────────────────────────────────────────────────────────

void ARGPlayerController::OnMapClicked(FRGGeoCoordinate Coordinate)
{
    if (bGuessSubmitted)
    {
        return; // Guess already locked for this round
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMapSubsystem* MapSub = GI->GetSubsystem<URGMapSubsystem>())
        {
            MapSub->PlaceGuess(Coordinate);
        }
    }
}

void ARGPlayerController::ConfirmGuess()
{
    if (bGuessSubmitted)
    {
        UE_LOG(LogMatch, Warning, TEXT("ConfirmGuess called but guess already submitted"));
        return;
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMapSubsystem* MapSub = GI->GetSubsystem<URGMapSubsystem>())
        {
            if (!MapSub->HasPendingGuess())
            {
                UE_LOG(LogMatch, Warning, TEXT("ConfirmGuess called but no pending guess placed"));
                return;
            }

            // TODO: Get round token from GameState
            const FString RoundToken = TEXT(""); // Will be populated from ARGGameState
            Server_SubmitGuess(MapSub->GetPendingGuess(), RoundToken);
            bGuessSubmitted = true;
        }
    }
}

// ─── Server RPC ───────────────────────────────────────────────────────────────

bool ARGPlayerController::Server_SubmitGuess_Validate(FRGGeoCoordinate Coordinate,
                                                       const FString& RoundToken)
{
    // Basic range validation — server does the authoritative check
    const bool bValidLat = Coordinate.Latitude  >= -90.0  && Coordinate.Latitude  <= 90.0;
    const bool bValidLon = Coordinate.Longitude >= -180.0 && Coordinate.Longitude <= 180.0;
    return bValidLat && bValidLon && !RoundToken.IsEmpty();
}

void ARGPlayerController::Server_SubmitGuess_Implementation(FRGGeoCoordinate Coordinate,
                                                             const FString& RoundToken)
{
    UE_LOG(LogMatch, Log, TEXT("Server received guess: lat=%.4f lon=%.4f token=%s"),
        Coordinate.Latitude, Coordinate.Longitude, *RoundToken);
    // TODO: Forward to GameMode / MatchSubsystem for authoritative scoring
}
