// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGGlobePawn.h"
#include "RadioGuesser.h"
#include "Map/RGCesiumMapManager.h"
#include "Player/RGPlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"

ARGGlobePawn::ARGGlobePawn()
{
    PrimaryActorTick.bCanEverTick = true;

    GlobeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GlobeRoot"));
    SetRootComponent(GlobeRoot);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GlobeRoot);
    SpringArm->TargetArmLength  = CurrentArmLength;
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = true;
    SpringArm->CameraLagSpeed   = 8.0f;
    SpringArm->bInheritPitch    = true;
    SpringArm->bInheritYaw      = true;
    SpringArm->bInheritRoll     = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    // Create input assets in constructor — they MUST exist when
    // SetupPlayerInputComponent runs (which is called before BeginPlay).
    MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Globe"));

    IA_Drag    = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeDrag"));
    IA_Drag->ValueType = EInputActionValueType::Boolean;

    IA_Zoom    = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeZoom"));
    IA_Zoom->ValueType = EInputActionValueType::Axis1D;

    IA_MouseXY = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeMouseXY"));
    IA_MouseXY->ValueType = EInputActionValueType::Axis2D;

    // Map keys once at CDO time
    MappingContext->MapKey(IA_Drag,    EKeys::LeftMouseButton);
    MappingContext->MapKey(IA_Zoom,    EKeys::MouseWheelAxis);
    MappingContext->MapKey(IA_MouseXY, EKeys::Mouse2D);
}

void ARGGlobePawn::BeginPlay()
{
    Super::BeginPlay();

    SetActorLocation(FVector::ZeroVector);
    SpringArm->TargetArmLength = CurrentArmLength;
    SpringArm->SetRelativeRotation(FRotator(-70.0f, 0.0f, 0.0f));

    // Register IMC with the Enhanced Input subsystem
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

    // WASD pan — legacy axis bindings work reliably in GameOnly mode
    PlayerInputComponent->BindAxis("MoveForward", this, &ARGGlobePawn::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight",   this, &ARGGlobePawn::MoveRight);
}

void ARGGlobePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Smooth zoom
    if (SpringArm)
    {
        SpringArm->TargetArmLength = FMath::FInterpTo(
            SpringArm->TargetArmLength, CurrentArmLength, DeltaTime, 8.0f);
    }

    // Mouse drag — rotate spring arm to pan the map
    if (bIsDragging && !LastMouseDelta.IsNearlyZero())
    {
        const FRotator CurrentRot = SpringArm->GetRelativeRotation();
        float NewYaw   = CurrentRot.Yaw   - LastMouseDelta.X * RotationSpeed;
        float NewPitch = FMath::Clamp(
            CurrentRot.Pitch + LastMouseDelta.Y * RotationSpeed,
            -89.0f, -15.0f);
        SpringArm->SetRelativeRotation(FRotator(NewPitch, NewYaw, 0.0f));
        LastMouseDelta = FVector2D::ZeroVector;
    }
}

// ─── Input handlers ───────────────────────────────────────────────────────────

void ARGGlobePawn::OnDragStarted(const FInputActionValue& /*Value*/)
{
    bIsDragging    = true;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnDragStopped(const FInputActionValue& /*Value*/)
{
    bIsDragging    = false;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnZoom(const FInputActionValue& Value)
{
    const float Axis = Value.Get<float>();
    CurrentArmLength = FMath::Clamp(
        CurrentArmLength - Axis * ZoomSpeed,
        MinArmLength,
        MaxArmLength);
}

void ARGGlobePawn::OnMouseXY(const FInputActionValue& Value)
{
    LastMouseDelta = Value.Get<FVector2D>();
}

// ─── WASD pan ─────────────────────────────────────────────────────────────────

void ARGGlobePawn::MoveForward(float Value)
{
    // W = вперёд = север = уменьшаем Yaw нет, это панорамирование по глобусу.
    // Мы вращаем SpringArm: Pitch вверх/вниз, Yaw влево/вправо.
    // MoveForward (W/S) — меняем Pitch (наклон к югу/северу)
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER || !SpringArm) return;
    const FRotator Rot = SpringArm->GetRelativeRotation();
    float NewPitch = FMath::Clamp(Rot.Pitch - Value * KeyboardPanSpeed, -89.0f, -15.0f);
    SpringArm->SetRelativeRotation(FRotator(NewPitch, Rot.Yaw, 0.0f));
}

void ARGGlobePawn::MoveRight(float Value)
{
    // A/D — меняем Yaw (поворот запад/восток)
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER || !SpringArm) return;
    const FRotator Rot = SpringArm->GetRelativeRotation();
    float NewYaw = Rot.Yaw + Value * KeyboardPanSpeed;
    SpringArm->SetRelativeRotation(FRotator(Rot.Pitch, NewYaw, 0.0f));
}
