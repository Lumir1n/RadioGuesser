// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGPlayerController.h"
#include "RadioGuesser.h"
#include "Map/RGMapSubsystem.h"
#include "Map/RGCesiumMapManager.h"
#include "Match/RGMatchSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

ARGPlayerController::ARGPlayerController()
{
    bShowMouseCursor       = true;
    bEnableClickEvents     = true;
    bEnableMouseOverEvents = true;
}

void ARGPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bGuessSubmitted = false;

    // Register Enhanced Input mapping context
    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSys =
            LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (DefaultMappingContext)
            {
                InputSys->AddMappingContext(DefaultMappingContext, 0);
                UE_LOG(LogMatch, Log, TEXT("Enhanced Input mapping context added"));
            }
            else
            {
                UE_LOG(LogMatch, Warning,
                    TEXT("ARGPlayerController: DefaultMappingContext not set. "
                         "Assign IMC_RadioGuesser in BP_RadioGuesserPlayerController defaults."));
            }
        }
    }
}

void ARGPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (MapClickAction)
        {
            EIC->BindAction(MapClickAction,    ETriggerEvent::Triggered,
                this, &ARGPlayerController::OnMapClick);
        }
        if (ConfirmGuessAction)
        {
            EIC->BindAction(ConfirmGuessAction, ETriggerEvent::Triggered,
                this, &ARGPlayerController::OnConfirmGuess);
        }
    }
    else
    {
        UE_LOG(LogMatch, Warning,
            TEXT("ARGPlayerController: InputComponent is not UEnhancedInputComponent. "
                 "Check Project Settings → Input → Default Input Component Class."));
    }
}

// ─── Input handlers ───────────────────────────────────────────────────────────

void ARGPlayerController::OnMapClick(const FInputActionValue& /*Value*/)
{
    TryPlaceGuessAtCursor();
}

void ARGPlayerController::OnConfirmGuess(const FInputActionValue& /*Value*/)
{
    ConfirmGuess();
}

// ─── Game actions ─────────────────────────────────────────────────────────────

void ARGPlayerController::TryPlaceGuessAtCursor()
{
    // Don't allow clicking after guess is locked
    if (bGuessSubmitted) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>();
    if (!Match || Match->GetMatchState() != ERGMatchState::RoundActive) return;

    // Find the CesiumMapManager in the level
    ARGCesiumMapManager* MapManager = Cast<ARGCesiumMapManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ARGCesiumMapManager::StaticClass()));

    if (!MapManager)
    {
        UE_LOG(LogMap, Warning,
            TEXT("TryPlaceGuessAtCursor: No ARGCesiumMapManager in level. "
                 "Place one in the WorldMap level."));
        return;
    }

    // Raycast from mouse cursor into the scene
    FHitResult HitResult;
    const bool bHit = GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        HitResult
    );

    if (bHit && HitResult.IsValidBlockingHit())
    {
        MapManager->HandleMapClick(HitResult.ImpactPoint);
        UE_LOG(LogMap, Verbose, TEXT("Map click hit at world pos: %s"),
            *HitResult.ImpactPoint.ToString());
    }
    else
    {
        UE_LOG(LogMap, Verbose, TEXT("Map click — no hit"));
    }
}

void ARGPlayerController::ConfirmGuess()
{
    if (bGuessSubmitted) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    URGMapSubsystem*  Map   = GI->GetSubsystem<URGMapSubsystem>();
    URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>();

    if (!Map || !Match) return;
    if (!Map->HasPendingGuess())
    {
        UE_LOG(LogMatch, Warning, TEXT("ConfirmGuess: no guess placed yet"));
        return;
    }
    if (Match->GetMatchState() != ERGMatchState::RoundActive)
    {
        UE_LOG(LogMatch, Warning, TEXT("ConfirmGuess: round not active"));
        return;
    }

    bGuessSubmitted = true;

    const FString RoundToken = Match->GetCurrentRound().RoundToken;
    // In multiplayer this goes via Server RPC; in solo we submit directly
    Server_SubmitGuess(Map->GetPendingGuess(), RoundToken);
}

// ─── Server RPC ───────────────────────────────────────────────────────────────

bool ARGPlayerController::Server_SubmitGuess_Validate(FRGGeoCoordinate Coordinate,
                                                       const FString& RoundToken)
{
    const bool bLatOK = Coordinate.Latitude  >= -90.0  && Coordinate.Latitude  <= 90.0;
    const bool bLonOK = Coordinate.Longitude >= -180.0 && Coordinate.Longitude <= 180.0;
    return bLatOK && bLonOK && !RoundToken.IsEmpty();
}

void ARGPlayerController::Server_SubmitGuess_Implementation(FRGGeoCoordinate Coordinate,
                                                             const FString& /*RoundToken*/)
{
    // In solo mode the server and client are the same process;
    // here we forward directly to MatchSubsystem.
    // In dedicated-server multiplayer this will call the authoritative GameMode.
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
    {
        Match->SubmitGuess(Coordinate);
    }

    UE_LOG(LogMatch, Log, TEXT("Server_SubmitGuess: lat=%.4f lon=%.4f"),
        Coordinate.Latitude, Coordinate.Longitude);
}
