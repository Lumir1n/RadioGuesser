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

// ─────────────────────────────────────────────────────────────────────────────
// Globe camera — architecture overview
//
// We NEVER call SetOriginLongitudeLatitudeHeight after BeginPlay.
// Constantly rebasing Cesium's world origin causes the tile tree to reload,
// which manifests as two continents appearing on top of each other.
//
// Instead:
//   • Origin is fixed at (lon=0, lat=0, h=0) (prime meridian / equator).
//   • The camera position is computed frame-by-frame via:
//       TransformLongitudeLatitudeHeightPositionToUnreal(ViewLon, ViewLat, H)
//     This works across the whole globe because Cesium stores its coordinate
//     frame relative to the fixed origin, not relative to a moving look-at.
//   • Drag panning updates ViewLat/ViewLon in geographic space (degrees).
//   • Zoom changes CurrentArmLength (height above ellipsoid in cm).
//
// Why this works: The UE floating-point precision issue only matters if the
// camera world position is hundreds of thousands of km from UE origin in
// metres. At our altitudes (100 km – 30 000 km), the ECEF position of any
// point on Earth is within ~50 000 km of UE origin — perfectly representable
// in single-precision float (precision ~4 m at that distance, fine for tiles).
// ─────────────────────────────────────────────────────────────────────────────

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

    // ── Fix Cesium origin at the prime meridian / equator — ONCE, never again.
    // All subsequent camera positioning is done by computing UE world coords
    // from geographic coords, without ever changing this origin.
    if (CachedGeoreference)
    {
        CachedGeoreference->SetOriginLongitudeLatitudeHeight(FVector(0.0, 0.0, 0.0));
    }

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

void ARGGlobePawn::ApplyCameraToGlobe()
{
    if (!CachedGeoreference) return;

    // Compute the UE world position of the camera directly from geographic coords.
    // No origin rebase — the georeference is fixed at (0,0,0).
    const double HeightM = static_cast<double>(CurrentArmLength) * 0.01; // cm → m
    const FVector CamPos = CachedGeoreference->TransformLongitudeLatitudeHeightPositionToUnreal(
        FVector(ViewLongitude, ViewLatitude, HeightM));
    SetActorLocation(CamPos);

    // Orient the camera to look straight down (nadir) with north at screen top.
    // In Cesium's ENU frame at any surface point:
    //   East  = local +X,  South = local +Y,  Up = local +Z
    // "Nadir" (straight down) = forward along -Up in ENU = (0, 0, -1) in ENU.
    // "North at top"          = screen-up along -South in ENU = (0, -1, 0) in ENU.
    // TransformEastSouthUpRotatorToUnreal converts this to UE world rotation
    // correctly for any latitude/longitude.
    const FRotator EsuNadirNorthUp = FRotationMatrix::MakeFromXZ(
        FVector(0.0, 0.0, -1.0),  // forward = -Up (nadir)
        FVector(0.0, -1.0, 0.0)   // screen-up = -South (north)
    ).Rotator();
    SetActorRotation(
        CachedGeoreference->TransformEastSouthUpRotatorToUnreal(EsuNadirNorthUp, CamPos));
}

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

    // ── Primary: grab-to-cursor.
    // Record a geographic point on LMB press; keep it under the cursor while dragging.
    if (bHasGrabPoint)
    {
        FHitResult Hit;
        const bool bHit = PC->GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit);

        if (bHit && Hit.bBlockingHit)
        {
            const FVector HitLLH =
                CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(Hit.ImpactPoint);

            ViewLongitude = WrapLongitude(ViewLongitude + (GrabLongitude - HitLLH.X));
            ViewLatitude  = FMath::Clamp(
                ViewLatitude + (HitLLH.Y - GrabLatitude) * static_cast<double>(NsScale),
                -static_cast<double>(MaxAbsLatitude),
                static_cast<double>(MaxAbsLatitude));
            return;
        }
    }

    if (Delta.IsNearlyZero()) return;

    // ── Fallback: pixel-delta pan (used when the cursor misses the terrain mesh).
    int32 SizeX = 1920, SizeY = 1080;
    PC->GetViewportSize(SizeX, SizeY);
    SizeY = FMath::Max(SizeY, 1);

    const double HalfFovRad     = FMath::DegreesToRadians((Camera ? Camera->FieldOfView : 60.0f) * 0.5);
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
