// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGPlayerController.h"
#include "RadioGuesser.h"
#include "Map/RGMapSubsystem.h"
#include "Map/RGCesiumMapManager.h"
#include "Match/RGMatchSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"

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

    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSys =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
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
                         "Assign IMC_RadioGuesser in BP defaults."));
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
            EIC->BindAction(MapClickAction,     ETriggerEvent::Triggered,
                this, &ARGPlayerController::OnMapClick);
        }
        if (ConfirmGuessAction)
        {
            EIC->BindAction(ConfirmGuessAction, ETriggerEvent::Triggered,
                this, &ARGPlayerController::OnConfirmGuess);
        }
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
    if (bGuessSubmitted) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>();
    if (!Match || Match->GetMatchState() != ERGMatchState::RoundActive) return;

    // Find ARGCesiumMapManager in the level
    AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), ARGCesiumMapManager::StaticClass());
    ARGCesiumMapManager* MapManager = Cast<ARGCesiumMapManager>(FoundActor);

    if (!MapManager)
    {
        UE_LOG(LogMap, Warning, TEXT("TryPlaceGuessAtCursor: No ARGCesiumMapManager in level."));
        return;
    }

    FHitResult HitResult;
    const bool bHit = GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        HitResult
    );

    if (bHit && HitResult.IsValidBlockingHit())
    {
        MapManager->HandleMapClick(HitResult.ImpactPoint);
    }
}

void ARGPlayerController::ConfirmGuess()
{
    if (bGuessSubmitted) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    URGMapSubsystem*   Map   = GI->GetSubsystem<URGMapSubsystem>();
    URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>();
    if (!Map || !Match) return;

    if (!Map->HasPendingGuess())
    {
        UE_LOG(LogMatch, Warning, TEXT("ConfirmGuess: no guess placed yet"));
        return;
    }
    if (Match->GetMatchState() != ERGMatchState::RoundActive) return;

    bGuessSubmitted = true;
    Server_SubmitGuess(Map->GetPendingGuess(), Match->GetCurrentRound().RoundToken);
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
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
    {
        Match->SubmitGuess(Coordinate);
    }
}
