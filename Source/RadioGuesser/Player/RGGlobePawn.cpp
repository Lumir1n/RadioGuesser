// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGGlobePawn.h"
#include "RadioGuesser.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/EngineTypes.h"
#include "CesiumGeoreference.h"

namespace
{
    constexpr double MetersPerDegreeLat = 111320.0;

    double WrapLongitude(double Lon)
    {
        Lon = FMath::Fmod(Lon + 180.0, 360.0);
        if (Lon < 0.0) Lon += 360.0;
        return Lon - 180.0;
    }
}

ARGGlobePawn::ARGGlobePawn()
{
    PrimaryActorTick.bCanEverTick = true;

    GlobeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GlobeRoot"));
    SetRootComponent(GlobeRoot);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GlobeRoot);
    SpringArm->TargetArmLength         = 0.0f;
    SpringArm->bDoCollisionTest        = false;
    SpringArm->bEnableCameraLag        = false;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bInheritPitch           = true;
    SpringArm->bInheritYaw             = true;
    SpringArm->bInheritRoll            = true;
    SpringArm->SetRelativeRotation(FRotator::ZeroRotator);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
    Camera->SetFieldOfView(60.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Globe"));

    IA_Drag    = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeDrag"));
    IA_Drag->ValueType = EInputActionValueType::Boolean;

    IA_Zoom    = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeZoom"));
    IA_Zoom->ValueType = EInputActionValueType::Axis1D;

    IA_MouseXY = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeMouseXY"));
    IA_MouseXY->ValueType = EInputActionValueType::Axis2D;

    MappingContext->MapKey(IA_Drag,    EKeys::LeftMouseButton);
    MappingContext->MapKey(IA_Zoom,    EKeys::MouseWheelAxis);
    MappingContext->MapKey(IA_MouseXY, EKeys::Mouse2D);
}

void ARGGlobePawn::BeginPlay()
{
    Super::BeginPlay();

    CachedGeoreference = FindGeoreference();
    CurrentArmLength   = FMath::Clamp(CurrentArmLength, MinArmLength, MaxArmLength);

    // Force immediate rebase on first frame
    AppliedOriginLatitude  = TNumericLimits<double>::Max();
    AppliedOriginLongitude = TNumericLimits<double>::Max();

    ApplyCameraToGlobe();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Sys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                Sys->AddMappingContext(MappingContext, 0);
            }
        }
    }
}

void ARGGlobePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EIC->BindAction(IA_Drag,    ETriggerEvent::Started,   this, &ARGGlobePawn::OnDragStarted);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Completed, this, &ARGGlobePawn::OnDragStopped);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Canceled,  this, &ARGGlobePawn::OnDragStopped);
        EIC->BindAction(IA_Zoom,    ETriggerEvent::Triggered, this, &ARGGlobePawn::OnZoom);
        EIC->BindAction(IA_MouseXY, ETriggerEvent::Triggered, this, &ARGGlobePawn::OnMouseXY);
    }

    PlayerInputComponent->BindAxis("MoveForward", this, &ARGGlobePawn::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight",   this, &ARGGlobePawn::MoveRight);
}

void ARGGlobePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CachedGeoreference)
    {
        CachedGeoreference = FindGeoreference();
    }

    if (bIsDragging)
    {
        ApplyDragPan();
    }

    ApplyCameraToGlobe();
}

ACesiumGeoreference* ARGGlobePawn::FindGeoreference() const
{
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
            return *It;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyCameraToGlobe
//
// Strategy: keep Cesium origin ALWAYS at (ViewLon, ViewLat, 0).
// This means the camera in UE space is always at (0, 0, HeightUE) — a tiny
// number relative to the Cesium internal precision budget.  No LOD seams.
//
// Rebase is gated on a minimum angular delta so we don't rebuild the tile
// tree every frame while the user is sitting still.  Once we trigger a rebase
// we ALSO move the camera on the same frame, so both happen atomically.
// ─────────────────────────────────────────────────────────────────────────────
void ARGGlobePawn::ApplyCameraToGlobe()
{
    if (!CachedGeoreference) return;

    // ── 1. Rebase origin when look-at drifted far enough ─────────────────────
    //   Threshold: 0.5° ~ 55 km at equator.  Small enough that UE floats stay
    //   precise; large enough that we don't rebuild tiles on every tiny pan.
    const double dLon = FMath::Abs(WrapLongitude(ViewLongitude - AppliedOriginLongitude));
    const double dLat = FMath::Abs(ViewLatitude  - AppliedOriginLatitude);
    const bool bFirst = (AppliedOriginLongitude > 1.0e30);

    if (bFirst || dLon > 0.5 || dLat > 0.5)
    {
        CachedGeoreference->SetOriginLongitudeLatitudeHeight(
            FVector(ViewLongitude, ViewLatitude, 0.0));
        AppliedOriginLongitude = ViewLongitude;
        AppliedOriginLatitude  = ViewLatitude;
    }

    // ── 2. Place camera directly above the (now-current) origin ──────────────
    //   After rebase, (ViewLon, ViewLat, 0) == UE origin.
    //   One metre above that == (ViewLon, ViewLat, 1 m) in geographic space.
    //   TransformLongitudeLatitudeHeightPositionToUnreal gives the exact UE pos.
    //
    //   Using height=HeightMeters puts the camera above the surface along the
    //   true ellipsoid normal — this is what eliminates the "horizontal slice"
    //   artefact that appeared when we used FVector(0,0,Height) in world space.
    const double HeightM = static_cast<double>(CurrentArmLength) * 0.01; // cm→m
    const FVector CamPos = CachedGeoreference->TransformLongitudeLatitudeHeightPositionToUnreal(
        FVector(ViewLongitude, ViewLatitude, HeightM));
    SetActorLocation(CamPos);

    // ── 3. Orient camera: nadir (look straight down), north up ───────────────
    //   In Cesium's ENU frame: East=+X, South=+Y, Up=+Z.
    //   "Look straight down" in ENU = forward along -Up = (0, 0, -1).
    //   "Screen-up = north"  in ENU = up along -South = (0, -1, 0).
    //   TransformEastSouthUpRotatorToUnreal converts this to Unreal world space
    //   correctly for any lat/lon (including high latitudes and poles).
    const FRotator EsuNadirNorthUp = FRotationMatrix::MakeFromXZ(
        FVector(0.0, 0.0, -1.0),   // forward = -Up (nadir)
        FVector(0.0, -1.0, 0.0)    // up = -South (north)
    ).Rotator();
    SetActorRotation(
        CachedGeoreference->TransformEastSouthUpRotatorToUnreal(EsuNadirNorthUp, CamPos));
}

// ─────────────────────────────────────────────────────────────────────────────

void ARGGlobePawn::ApplyDragPan()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !CachedGeoreference)
    {
        LastMouseDelta = FVector2D::ZeroVector;
        return;
    }

    FVector2D CursorPos = LastCursorPos;
    const bool bGotCursor = PC->GetMousePosition(CursorPos.X, CursorPos.Y);

    FVector2D Delta = LastMouseDelta;
    LastMouseDelta = FVector2D::ZeroVector;

    if (bGotCursor)
    {
        if (bHaveCursorPos)
            Delta = CursorPos - LastCursorPos;
        LastCursorPos  = CursorPos;
        bHaveCursorPos = true;
    }

    TotalDragPixels += Delta.Size();
    if (TotalDragPixels >= ClickDragThreshold)
        bDragExceededThreshold = true;

    const float NsScale = GetNorthSouthPanScale();

    // Grab-to-cursor: the geo-point under the cursor at drag-start follows it.
    if (bHasGrabPoint)
    {
        FHitResult Hit;
        const bool bHit = PC->GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit);

        if (bHit && Hit.bBlockingHit)
        {
            const FVector HitLLH =
                CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(Hit.ImpactPoint);

            // Longitude: keep the grab point under the cursor (no scale)
            ViewLongitude = WrapLongitude(ViewLongitude + (GrabLongitude - HitLLH.X));
            // Latitude:  scale by NsScale (fade out N/S drag at globe view)
            ViewLatitude  = FMath::Clamp(
                ViewLatitude + (HitLLH.Y - GrabLatitude) * static_cast<double>(NsScale),
                -static_cast<double>(MaxAbsLatitude),
                static_cast<double>(MaxAbsLatitude));
            return;
        }
    }

    if (Delta.IsNearlyZero()) return;

    // Fallback: pixel-to-meter drag when no surface hit is available
    int32 SizeX = 1920, SizeY = 1080;
    PC->GetViewportSize(SizeX, SizeY);
    SizeY = FMath::Max(SizeY, 1);

    const double HalfFovRad    = FMath::DegreesToRadians((Camera ? Camera->FieldOfView : 60.0f) * 0.5);
    const double MetersPerPixel = (2.0 * GetViewHeightMeters() * FMath::Tan(HalfFovRad))
                                  / static_cast<double>(SizeY);

    PanByMeters(-static_cast<double>(Delta.X) * MetersPerPixel,
                 static_cast<double>(Delta.Y) * MetersPerPixel * NsScale);
}

void ARGGlobePawn::PanByMeters(double EastMeters, double NorthMeters)
{
    const double CosLat = FMath::Max(0.05,
        FMath::Abs(FMath::Cos(FMath::DegreesToRadians(ViewLatitude))));

    ViewLatitude = FMath::Clamp(
        ViewLatitude + NorthMeters / MetersPerDegreeLat,
        -static_cast<double>(MaxAbsLatitude),
        static_cast<double>(MaxAbsLatitude));

    ViewLongitude = WrapLongitude(
        ViewLongitude + EastMeters / (MetersPerDegreeLat * CosLat));
}

float ARGGlobePawn::GetNorthSouthPanScale() const
{
    const float Span  = FMath::Max(1.0f, PlanetViewFullHeight - PlanetViewStartHeight);
    const float Alpha = FMath::Clamp((CurrentArmLength - PlanetViewStartHeight) / Span, 0.0f, 1.0f);
    return 1.0f - Alpha;
}

// ─── Input ───────────────────────────────────────────────────────────────────

void ARGGlobePawn::OnDragStarted(const FInputActionValue& /*Value*/)
{
    bIsDragging            = true;
    bDragExceededThreshold = false;
    bHasGrabPoint          = false;
    TotalDragPixels        = 0.0f;
    LastMouseDelta         = FVector2D::ZeroVector;
    bHaveCursorPos         = false;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !CachedGeoreference) return;

    if (PC->GetMousePosition(LastCursorPos.X, LastCursorPos.Y))
        bHaveCursorPos = true;

    // Try to record the geographic grab point for grab-to-cursor panning
    FHitResult Hit;
    if (PC->GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)
        && Hit.bBlockingHit)
    {
        const FVector LLH =
            CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(Hit.ImpactPoint);
        GrabLongitude = LLH.X;
        GrabLatitude  = LLH.Y;
        bHasGrabPoint = true;
    }
}

void ARGGlobePawn::OnDragStopped(const FInputActionValue& /*Value*/)
{
    bIsDragging    = false;
    bHasGrabPoint  = false;
    bHaveCursorPos = false;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnZoom(const FInputActionValue& Value)
{
    const float Axis      = Value.Get<float>();
    const float ZoomDelta = CurrentArmLength * 0.15f * Axis;
    CurrentArmLength = FMath::Clamp(CurrentArmLength - ZoomDelta, MinArmLength, MaxArmLength);
}

void ARGGlobePawn::OnMouseXY(const FInputActionValue& Value)
{
    LastMouseDelta = Value.Get<FVector2D>();
}

// ─── WASD ─────────────────────────────────────────────────────────────────────

void ARGGlobePawn::MoveForward(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER) return;
    const float NsScale = GetNorthSouthPanScale();
    if (NsScale <= KINDA_SMALL_NUMBER) return;
    const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    PanByMeters(0.0, static_cast<double>(Value) * KeyboardPanSpeed * GetViewHeightMeters() * Dt * NsScale);
}

void ARGGlobePawn::MoveRight(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER) return;
    const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    PanByMeters(static_cast<double>(Value) * KeyboardPanSpeed * GetViewHeightMeters() * Dt, 0.0);
}

// ─── Focus after round result ─────────────────────────────────────────────────

void ARGGlobePawn::FocusOnGuessResult(FRGGeoCoordinate Guess, FRGGeoCoordinate Actual, float DistanceKm)
{
    ViewLatitude = FMath::Clamp(
        (Guess.Latitude + Actual.Latitude) * 0.5,
        -static_cast<double>(MaxAbsLatitude),
        static_cast<double>(MaxAbsLatitude));

    double LonA = Guess.Longitude;
    double LonB = Actual.Longitude;
    const double DLon = LonB - LonA;
    if      (DLon >  180.0) LonB -= 360.0;
    else if (DLon < -180.0) LonB += 360.0;
    ViewLongitude = WrapLongitude((LonA + LonB) * 0.5);

    const float HeightKm = FMath::Clamp(FMath::Max(DistanceKm * 2.0f, 400.0f), 400.0f, 20000.0f);
    CurrentArmLength = FMath::Clamp(HeightKm * 100000.0f, MinArmLength, MaxArmLength);
}
