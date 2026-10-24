#pragma once

#include "CoreMinimal.h"

/** Buttons of an N64 controller. */
enum class EN64 : uint8
{
	A, B, Z, Start, L, R, CUp, CDown, CLeft, CRight, DUp, DDown, DLeft, DRight,
	Count
};

/**
 * A USB N64 controller, read straight from the Linux joystick device. USB N64 pads show up as
 * generic "DirectInput" joysticks whose button numbers differ from adapter to adapter, so
 * Unreal's gamepad layer can't make sense of them. The default layout is the ShanWan / "SWITCH
 * CO.,LTD." adapter's: A=1 B=2 Z=6 Start=12 L=4 R=5, C-Up=9 C-Down=0 C-Left=3 C-Right=8,
 * stick on axes 0/1, D-pad as a hat on axes 4/5.
 *
 * Any other controller can be calibrated in the game (FPadCalibrator); the result is saved per
 * controller name in Saved/SparkyLeo_Pads.ini and loaded whenever that controller is plugged
 * in. A calibrated controller is picked up even if its name doesn't look like an N64 pad's.
 *
 * -N64Pad=auto (default), -N64Pad=/dev/input/jsN or -N64Pad=off. A map by hand:
 * -N64Map=A:1,B:2,Z:6,Start:12,L:4,R:5,CU:9,CD:0,CL:3,CR:8 (js button numbers; find them with
 * jstest; "a5-" for an axis and direction, as in DU:a5-).
 */
class FN64Pad
{
public:
	/** Where an N64 button comes from on this controller. */
	struct FBind
	{
		enum EKind : uint8 { None, Button, AxisPos, AxisNeg };
		EKind Kind = None;
		uint8 Index = 0;
		bool operator==(const FBind& O) const { return Kind == O.Kind && (Kind == None || Index == O.Index); }
	};
	static constexpr int32 MaxAxes = 16;

	FN64Pad();
	~FN64Pad() { Close(); }

	bool Open(const FString& Spec);
	/** Opens one device whatever it's called (the calibrator does this). */
	bool OpenAny(const FString& DevPath) { return OpenDevice(DevPath, false); }
	void Close();

	/** Reads pending events. Returns false if the device went away. */
	bool Poll();

	bool IsOpen() const { return Fd >= 0; }
	const FString& GetName() const { return Name; }
	const FString& GetPath() const { return Path; }

	/** Analog stick, -1..1, +X right, +Y up, with a dead zone (N64 sticks rarely centre exactly). */
	FVector2D GetStick() const;

	bool IsDown(EN64 B) const;
	/** True once per press. */
	bool WasPressed(EN64 B);

	/** Applies a map spec (-N64Map=, or a saved calibration). Parts it doesn't mention keep their defaults. */
	void SetMap(const FString& Spec);
	FString GetMap() const;
	void ResetMap();

	// raw access, for calibrating
	int32 GetRawAxis(int32 I) const { return I >= 0 && I < MaxAxes ? Axis[I] : 0; }
	int32 GetNumAxes() const { return NumAxes; }
	uint32 GetRawButtons() const { return ButtonsDown; }

	// calibrations saved per controller name
	static FString LoadSaved(const FString& DeviceName);
	static void Save(const FString& DeviceName, const FString& Spec);

private:
	int Fd = -1;
	FString Name;
	FString Path;
	int32 NumAxes = 0;
	int16 Axis[MaxAxes] = {};
	uint32 ButtonsDown = 0;          // js buttons
	uint32 Pressed = 0;              // EN64 bits, set on press
	uint32 AxisDown = 0;             // EN64 bits held through axis bindings
	FBind Bind[int32(EN64::Count)];
	int16 Rest[MaxAxes] = {};        // where each axis sits untouched (some pads rest axes at -32767)
	// the stick: which axes, which way is +, where the middle is, how far it really goes each way
	int32 StickAxis[2] = {0, 1};
	float StickSign[2] = {1.f, -1.f};
	float StickCenter[2] = {0.f, 0.f};
	float StickPos[2] = {32767.f, 32767.f};
	float StickNeg[2] = {32767.f, 32767.f};
	float DeadZone = 0.15f;

	bool OpenDevice(const FString& DevPath, bool bRequireN64Name);
	bool AxisBindDown(const FBind& B) const;
	void UpdateAxisBinds();
};

/**
 * Walks the player through setting up any controller: press A on it (whichever connected
 * controller that comes from is the one being set up), then every other button, then the
 * stick (let go; push right; push up; roll it round the edge). The result is saved for that
 * controller and loaded into the game's pad.
 */
class FPadCalibrator
{
public:
	void Start();
	void Stop();
	bool IsActive() const { return bActive; }
	/** Advance. Returns true when the calibration has just been saved (and loaded into Target). */
	bool Tick(float Dt, FN64Pad& Target);

	FString Prompt() const;
	FString Hint() const;
	int32 GetStep() const { return Step; }
	int32 GetNumSteps() const { return NumSteps; }
	float GetProgress() const { return Progress; }
	FString GetDeviceName() const { return Dev ? Dev->GetName() : FString(); }
	/** Live readout of the controller being set up: axes (-1..1) and held buttons. */
	void GetLive(TArray<float>& OutAxes, TArray<int32>& OutButtons) const;

private:
	enum EStepKind : uint8 { Choose, Buttons, StickRest, StickRight, StickUp, StickRoll, Confirm };
	static constexpr int32 FirstButtonStep = 1;  // steps 1..13: B .. D-Right (A is chosen in step 0)
	static constexpr int32 NumSteps = 1 + 13 + 4 + 1;

	bool bActive = false;
	int32 Step = 0;
	float StepTime = 0.f;
	float Progress = 0.f;
	bool bWaitRelease = false;
	TArray<TUniquePtr<FN64Pad>> Candidates;
	FN64Pad* Dev = nullptr;
	uint32 PrevButtons = 0;
	TArray<uint32> CandidatePrev;
	int16 Rest[FN64Pad::MaxAxes] = {};
	FN64Pad::FBind Found[int32(EN64::Count)];
	int32 StickAxis[2] = {-1, -1};
	float StickSign[2] = {1.f, 1.f};
	float StickMax[2] = {0.f, 0.f}, StickMin[2] = {0.f, 0.f};
	bool bRollStarted = false;

	EStepKind Kind() const;
	EN64 StepButton() const { return EN64(Step - FirstButtonStep + 1); }
	bool IsUsed(const FN64Pad::FBind& B) const;
	bool DetectInput(FN64Pad::FBind& Out);
	bool AllReleased() const;
	void CaptureRest();
	FString BuildSpec() const;
	void Next();
};
