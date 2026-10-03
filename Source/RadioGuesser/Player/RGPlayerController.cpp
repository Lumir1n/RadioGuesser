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
#include "CesiumGeoreference.h"
#include "CesiumWgs84Ellipsoid.h"
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

    // Game-and-UI input mode with viewport lock:
    // - cursor is visible (needed for GetHitResultUnderCursor + GetMousePosition in the pawn)
    // - mouse is locked inside the viewport so drag works correctly
    // - Slate does NOT intercept LMB, so both pawn drag AND controller click bindings fire
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
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
        if (ConfirmGuessAction)
        {
            EIC->BindAction(ConfirmGuessAction, ETriggerEvent::Triggered,
                this, &ARGPlayerController::OnConfirmGuess);
        }
    }

    // Use legacy input binding for LMB release — Enhanced Input conflicts with
    // the pawn's IA_GlobeDrag action which also listens on LMB.
    // IE_Released fires after the pawn's drag handler, so we still get the event.
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this,
        &ARGPlayerController::OnMapClickLegacy);

    // Bind Escape through the old input system (reliable in PIE, no IMC needed)
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this,
        &ARGPlayerController::OnEscapePressed);
}

// ─── Input handlers ───────────────────────────────────────────────────────────

void ARGPlayerController::OnMapClickLegacy()
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
    if (!Match)
    {
        UE_LOG(LogMap, Warning, TEXT("TryPlaceGuessAtCursor: No MatchSubsystem"));
        return;
    }
    if (Match->GetMatchState() != ERGMatchState::RoundActive)
    {
        UE_LOG(LogMap, Log, TEXT("TryPlaceGuessAtCursor: Match not active (state=%d)"),
            static_cast<int32>(Match->GetMatchState()));
        return;
    }

    if (const ARGGlobePawn* Globe = Cast<ARGGlobePawn>(GetPawn()))
    {
        if (Globe->DidDragExceedClickThreshold())
        {
            UE_LOG(LogMap, Log, TEXT("TryPlaceGuessAtCursor: drag exceeded threshold, skip"));
            return;
        }
    }

    if (!CachedMapManager)
    {
        UE_LOG(LogMap, Warning, TEXT("TryPlaceGuessAtCursor: No ARGCesiumMapManager in level."));
        return;
    }

    // ── Strategy 1: standard line trace (WorldDynamic / WorldStatic / Visibility) ──
    FHitResult HitResult;
    bool bHit = GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_WorldDynamic), true, HitResult);
    if (!bHit || !HitResult.IsValidBlockingHit())
        bHit = GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_WorldStatic), true, HitResult);
    if (!bHit || !HitResult.IsValidBlockingHit())
        bHit = GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_Visibility), true, HitResult);

    UE_LOG(LogMap, Log, TEXT("TryPlaceGuessAtCursor: traceHit=%d"), bHit && HitResult.IsValidBlockingHit());

    if (bHit && HitResult.IsValidBlockingHit())
    {
        CachedMapManager->HandleMapClick(HitResult.ImpactPoint);
        return;
    }

    // ── Strategy 2: ray-ellipsoid intersection (always works for globe camera) ──
    // Deproject the cursor pixel to a world-space ray.
    float MouseX, MouseY;
    if (!GetMousePosition(MouseX, MouseY))
    {
        UE_LOG(LogMap, Warning, TEXT("TryPlaceGuessAtCursor: GetMousePosition failed"));
        return;
    }

    FVector RayOriginUnreal, RayDirUnreal;
    if (!DeprojectScreenPositionToWorld(MouseX, MouseY, RayOriginUnreal, RayDirUnreal))
    {
        UE_LOG(LogMap, Warning, TEXT("TryPlaceGuessAtCursor: DeprojectScreenPositionToWorld failed"));
        return;
    }

    // Find CesiumGeoreference in the world
    ACesiumGeoreference* Georef = nullptr;
    for (TActorIterator<ACesiumGeoreference> It(GetWorld()); It; ++It)
    {
        Georef = *It;
        break;
    }
    if (!Georef)
    {
        UE_LOG(LogMap, Warning, TEXT("TryPlaceGuessAtCursor: No ACesiumGeoreference found"));
        return;
    }

    // Convert ray to ECEF (metres).
    const FVector RayOriginEcef = Georef->TransformUnrealPositionToEarthCenteredEarthFixed(RayOriginUnreal);
    const FVector RayDirEcef    = Georef->TransformUnrealDirectionToEarthCenteredEarthFixed(RayDirUnreal);

    // WGS84 semi-axes in metres
    const FVector Radii = UCesiumWgs84Ellipsoid::GetRadii(); // (a, a, b)
    const double  Ra    = Radii.X; // equatorial ~6378137 m
    const double  Rb    = Radii.Z; // polar      ~6356752 m

    // Solve: (Ox+t*Dx)²/Ra² + (Oy+t*Dy)²/Ra² + (Oz+t*Dz)²/Rb² = 1
    const double Dx = RayDirEcef.X,    Dy = RayDirEcef.Y,    Dz = RayDirEcef.Z;
    const double Ox = RayOriginEcef.X, Oy = RayOriginEcef.Y, Oz = RayOriginEcef.Z;
    const double Ra2 = Ra * Ra, Rb2 = Rb * Rb;

    const double A = Dx*Dx/Ra2 + Dy*Dy/Ra2 + Dz*Dz/Rb2;
    const double B = 2.0 * (Ox*Dx/Ra2 + Oy*Dy/Ra2 + Oz*Dz/Rb2);
    const double C = Ox*Ox/Ra2 + Oy*Oy/Ra2 + Oz*Oz/Rb2 - 1.0;

    const double Discriminant = B*B - 4.0*A*C;
    if (Discriminant < 0.0)
    {
        UE_LOG(LogMap, Log, TEXT("TryPlaceGuessAtCursor: ray misses ellipsoid (disc=%.2f)"), Discriminant);
        return;
    }

    const double SqrtD = FMath::Sqrt(Discriminant);
    const double T1    = (-B - SqrtD) / (2.0 * A);
    const double T2    = (-B + SqrtD) / (2.0 * A);
    double T = -1.0;
    if      (T1 > 0.0 && T2 > 0.0) T = FMath::Min(T1, T2);
    else if (T1 > 0.0)              T = T1;
    else if (T2 > 0.0)              T = T2;

    if (T < 0.0)
    {
        UE_LOG(LogMap, Log, TEXT("TryPlaceGuessAtCursor: ellipsoid intersection behind camera"));
        return;
    }

    const FVector HitEcef   = FVector(Ox + Dx*T, Oy + Dy*T, Oz + Dz*T);
    const FVector HitUnreal = Georef->TransformEarthCenteredEarthFixedPositionToUnreal(HitEcef);

    UE_LOG(LogMap, Log, TEXT("TryPlaceGuessAtCursor: ellipsoid hit OK → UE=(%.0f,%.0f,%.0f)"),
        HitUnreal.X, HitUnreal.Y, HitUnreal.Z);

    CachedMapManager->HandleMapClick(HitUnreal);
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
    // Toggle back to GameAndUI on Escape so Slate/editor regains focus in PIE.
    // In a shipped build this would open a pause menu.
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    bShowMouseCursor = true;

    UE_LOG(LogMatch, Log, TEXT("ARGPlayerController: Escape — released to GameAndUI mode."));
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
