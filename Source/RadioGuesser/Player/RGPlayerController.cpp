// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGPlayerController.h"
#include "RadioGuesser.h"
#include "Map/RGMapSubsystem.h"
#include "Map/RGCesiumMapManager.h"
#include "Match/RGMatchSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/EngineTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Player/RGGlobePawn.h"

ARGPlayerController::ARGPlayerController()
{
    // bShowMouseCursor is intentionally NOT set here.
    // It is controlled in BeginPlay based on input mode.
    bEnableClickEvents     = true;
    bEnableMouseOverEvents = true;
}

void ARGPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bGuessSubmitted = false;

    // Visible cursor: drag pans the globe under the pointer (Google Earth).
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        SetInputMode(Mode);
        bShowMouseCursor = true;
    }

    // Auto-load input assets if not assigned in Blueprint defaults
    if (!DefaultMappingContext)
    {
        DefaultMappingContext = LoadObject<UInputMappingContext>(
            nullptr, TEXT("/Game/Input/IMC_RadioGuesser.IMC_RadioGuesser"));
    }
    if (!MapClickAction)
    {
        MapClickAction = LoadObject<UInputAction>(
            nullptr, TEXT("/Game/Input/IA_MapClick.IA_MapClick"));
    }
    if (!ConfirmGuessAction)
    {
        ConfirmGuessAction = LoadObject<UInputAction>(
            nullptr, TEXT("/Game/Input/IA_ConfirmGuess.IA_ConfirmGuess"));
    }

    if (!DefaultMappingContext)
    {
        UE_LOG(LogMatch, Warning, TEXT("ARGPlayerController: IMC_RadioGuesser not found!"));
    }
    if (!MapClickAction)
    {
        UE_LOG(LogMatch, Warning, TEXT("ARGPlayerController: IA_MapClick not found!"));
    }

    // Cache map manager reference once at begin play
    for (TActorIterator<ARGCesiumMapManager> It(GetWorld()); It; ++It)
    {
        CachedMapManager = *It;
        break;
    }

    // Subscribe to match state so guess lock resets automatically each round
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
        {
            Match->OnMatchStateChanged.AddDynamic(this, &ARGPlayerController::HandleMatchStateChanged);
        }
    }

    // Register Enhanced Input mapping context
    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Sys =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
        {
            if (DefaultMappingContext)
            {
                Sys->AddMappingContext(DefaultMappingContext, 0);
                UE_LOG(LogMatch, Log, TEXT("ARGPlayerController: Input mapping context added."));
            }
            else
            {
                UE_LOG(LogMatch, Warning, TEXT("ARGPlayerController: DefaultMappingContext not set."));
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
            // Completed = mouse release, so a drag does not place a guess.
            EIC->BindAction(MapClickAction,     ETriggerEvent::Completed,
                this, &ARGPlayerController::OnMapClick);
        }
        if (ConfirmGuessAction)
        {
            EIC->BindAction(ConfirmGuessAction, ETriggerEvent::Triggered,
                this, &ARGPlayerController::OnConfirmGuess);
        }
    }

    // Bind Escape through the old input system (reliable in PIE, no IMC needed)
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this,
        &ARGPlayerController::OnEscapePressed);
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

    if (const ARGGlobePawn* Globe = Cast<ARGGlobePawn>(GetPawn()))
    {
        if (Globe->DidDragExceedClickThreshold())
        {
            return;
        }
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>();
    if (!Match || Match->GetMatchState() != ERGMatchState::RoundActive) return;

    if (!CachedMapManager)
    {
        UE_LOG(LogMap, Warning, TEXT("TryPlaceGuessAtCursor: No ARGCesiumMapManager in level."));
        return;
    }

    FHitResult HitResult;
    const bool bHit = GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        HitResult);

    if (bHit && HitResult.IsValidBlockingHit())
    {
        CachedMapManager->HandleMapClick(HitResult.ImpactPoint);
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

    const FString RoundToken = Match->GetCurrentRound().RoundToken;
    Server_SubmitGuess(Map->GetPendingGuess(), RoundToken);
}

void ARGPlayerController::ResetGuessLock()
{
    bGuessSubmitted = false;
}

void ARGPlayerController::HandleMatchStateChanged(ERGMatchState NewState)
{
    // Reset guess lock whenever a new round becomes active so the player
    // can place a fresh guess without needing a manual ResetGuessLock call.
    if (NewState == ERGMatchState::RoundActive)
    {
        bGuessSubmitted = false;
    }
}

// ─── Escape / focus toggle ────────────────────────────────────────────────────

void ARGPlayerController::OnEscapePressed()
{
    // Switch to GameAndUI so the OS cursor reappears and the editor/Slate
    // regains focus. The player can press Play again to re-enter game mode.
    // In a shipped build this would open a pause menu instead.
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    bShowMouseCursor = true;

    UE_LOG(LogMatch, Log, TEXT("ARGPlayerController: Escape pressed — releasing mouse capture."));
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

    UE_LOG(LogMatch, Log, TEXT("Server_SubmitGuess: lat=%.4f lon=%.4f"),
        Coordinate.Latitude, Coordinate.Longitude);
}
