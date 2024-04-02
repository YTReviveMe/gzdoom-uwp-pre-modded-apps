/*
** i_joystick.cpp
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
#include <SDL.h>

#include "basics.h"
#include "cmdlib.h"

#include "m_joy.h"
#include "keydef.h"
#ifdef _WINDOWS_UWP
#include <windows.h>
#endif // _WINDOSW_UWP



#define DEFAULT_DEADZONE 0.25f

// Very small deadzone so that floating point magic doesn't happen
#define MIN_DEADZONE 0.000001f

struct IConfigurableJoystick : public IJoystickConfig
{
	virtual bool IsValid() const = 0;
	virtual void AddAxes(float axes[NUM_JOYAXIS]) = 0;
	virtual void ProcessInput() = 0;
};

class SDLInputJoystick: public IConfigurableJoystick
{
public:
	SDLInputJoystick(int DeviceIndex) : DeviceIndex(DeviceIndex), Multiplier(1.0f) , Enabled(true)
	{
		Device = SDL_JoystickOpen(DeviceIndex);
		if(Device != NULL)
		{
			NumAxes = SDL_JoystickNumAxes(Device);
			NumHats = SDL_JoystickNumHats(Device);

			SetDefaultConfig();
		}
	}
	~SDLInputJoystick()
	{
		if(Device != NULL)
			M_SaveJoystickConfig(this);
		SDL_JoystickClose(Device);
	}

	bool IsValid() const
	{
		return Device != NULL;
	}

	FString GetName()
	{
		return SDL_JoystickName(Device);
	}
	float GetSensitivity()
	{
		return Multiplier;
	}
	void SetSensitivity(float scale)
	{
		Multiplier = scale;
	}

	int GetNumAxes()
	{
		return NumAxes + NumHats*2;
	}
	float GetAxisDeadZone(int axis)
	{
		return Axes[axis].DeadZone;
	}
	EJoyAxis GetAxisMap(int axis)
	{
		return Axes[axis].GameAxis;
	}
	const char *GetAxisName(int axis)
	{
		return Axes[axis].Name.GetChars();
	}
	float GetAxisScale(int axis)
	{
		return Axes[axis].Multiplier;
	}

	void SetAxisDeadZone(int axis, float zone)
	{
		Axes[axis].DeadZone = clamp(zone, MIN_DEADZONE, 1.f);
	}
	void SetAxisMap(int axis, EJoyAxis gameaxis)
	{
		Axes[axis].GameAxis = gameaxis;
	}
	void SetAxisScale(int axis, float scale)
	{
		Axes[axis].Multiplier = scale;
	}

	// Used by the saver to not save properties that are at their defaults.
	bool IsSensitivityDefault()
	{
		return Multiplier == 1.0f;
	}
	bool IsAxisDeadZoneDefault(int axis)
	{
		return Axes[axis].DeadZone <= MIN_DEADZONE;
	}
	bool IsAxisMapDefault(int axis)
	{
		if(axis >= 5)
			return Axes[axis].GameAxis == JOYAXIS_None;
		return Axes[axis].GameAxis == DefaultAxes[axis];
	}
	bool IsAxisScaleDefault(int axis)
	{
		return Axes[axis].Multiplier == 1.0f;
	}

	void SetDefaultConfig()
	{
		for(int i = 0;i < GetNumAxes();i++)
		{
			AxisInfo info;
			if(i < NumAxes)
				info.Name.Format("Axis %d", i+1);
			else
				info.Name.Format("Hat %d (%c)", (i-NumAxes)/2 + 1, (i-NumAxes)%2 == 0 ? 'x' : 'y');
			info.DeadZone = DEFAULT_DEADZONE;
			info.Multiplier = 1.0f;
			info.Value = 0.0;
			info.ButtonValue = 0;
			if(i >= 5)
				info.GameAxis = JOYAXIS_None;
			else
				info.GameAxis = DefaultAxes[i];
			Axes.Push(info);
		}
	}

	bool GetEnabled()
	{
		return Enabled;
	}
	
	void SetEnabled(bool enabled)
	{
		Enabled = enabled;
	}

	virtual FString GetIdentifier()
	{
		char id[16];
		snprintf(id, countof(id), "JS:%d", DeviceIndex);
		return id;
	}

	virtual void AddAxes(float axes[NUM_JOYAXIS])
	{
		// Add to game axes.
		for (int i = 0; i < GetNumAxes(); ++i)
		{
			if(Axes[i].GameAxis != JOYAXIS_None)
				axes[Axes[i].GameAxis] -= float(Axes[i].Value * Multiplier * Axes[i].Multiplier);
		}
	}

	void ProcessInput()
	{
		uint8_t buttonstate;

		for (int i = 0; i < NumAxes; ++i)
		{
			buttonstate = 0;

			Axes[i].Value = SDL_JoystickGetAxis(Device, i)/32767.0;
			Axes[i].Value = Joy_RemoveDeadZone(Axes[i].Value, Axes[i].DeadZone, &buttonstate);

			// Map button to axis
			// X and Y are handled differently so if we have 2 or more axes then we'll use that code instead.
			if (NumAxes == 1 || (i >= 2 && i < NUM_JOYAXISBUTTONS))
			{
				Joy_GenerateButtonEvents(Axes[i].ButtonValue, buttonstate, 2, KEY_JOYAXIS1PLUS + i*2);
				Axes[i].ButtonValue = buttonstate;
			}
		}

		if(NumAxes > 1)
		{
			buttonstate = Joy_XYAxesToButtons(Axes[0].Value, Axes[1].Value);
			Joy_GenerateButtonEvents(Axes[0].ButtonValue, buttonstate, 4, KEY_JOYAXIS1PLUS);
			Axes[0].ButtonValue = buttonstate;
		}

		// Map POV hats to buttons and axes.  Why axes?  Well apparently I have
		// a gamepad where the left control stick is a POV hat (instead of the
		// d-pad like you would expect, no that's pressure sensitive).  Also
		// KDE's joystick dialog maps them to axes as well.
		for (int i = 0; i < NumHats; ++i)
		{
			AxisInfo &x = Axes[NumAxes + i*2];
			AxisInfo &y = Axes[NumAxes + i*2 + 1];

			buttonstate = SDL_JoystickGetHat(Device, i);

			// If we're going to assume that we can pass SDL's value into
			// Joy_GenerateButtonEvents then we might as well assume the format here.
			if(buttonstate & 0x1) // Up
				y.Value = -1.0;
			else if(buttonstate & 0x4) // Down
				y.Value = 1.0;
			else
				y.Value = 0.0;
			if(buttonstate & 0x2) // Left
				x.Value = 1.0;
			else if(buttonstate & 0x8) // Right
				x.Value = -1.0;
			else
				x.Value = 0.0;

			if(i < 4)
			{
				Joy_GenerateButtonEvents(x.ButtonValue, buttonstate, 4, KEY_JOYPOV1_UP + i*4);
				x.ButtonValue = buttonstate;
			}
		}
	}

protected:
	struct AxisInfo
	{
		FString Name;
		float DeadZone;
		float Multiplier;
		EJoyAxis GameAxis;
		double Value;
		uint8_t ButtonValue;
	};
	static const EJoyAxis DefaultAxes[5];

	int					DeviceIndex;
	SDL_Joystick		*Device;

	float				Multiplier;
	bool				Enabled;
	TArray<AxisInfo>	Axes;
	int					NumAxes;
	int					NumHats;

	friend class SDLInputJoystickManager;
};

class SDLInputGamepad : public IConfigurableJoystick
{
	bool Enabled = true;
	SDL_GameController* _Gamepad = nullptr;

	//Configuration
	struct AxisInfo
	{
		float DeadZone;
		float Multiplier;
	};

	AxisInfo AxisSettings[SDL_CONTROLLER_AXIS_MAX] = { 0 };

	float				Multiplier;

	//ThumbSticks
	uint8_t XY_status = 0;
	uint8_t YawPitch_status = 0;
	
	//Triggers
	uint8_t Left_status = 0;
	uint8_t Right_status = 0;

	uint8_t DPAD_status = 0;
	uint8_t Buttons1_status = 0;
	uint8_t Buttons2_status = 0;

public:
	SDLInputGamepad(int DeviceIndex)
	{
		_Gamepad = SDL_GameControllerOpen(DeviceIndex);
	}

	virtual ~SDLInputGamepad()
	{
		SDL_GameControllerClose(_Gamepad);
	}

	virtual bool IsValid() const
	{
		return _Gamepad != nullptr;
	}

	virtual FString GetName()
	{
		return SDL_GameControllerName(_Gamepad);
	}

	virtual float GetSensitivity()
	{
		return Multiplier;
	}

	virtual void SetSensitivity(float scale)
	{
		Multiplier = scale;
	}

	virtual int GetNumAxes()
	{
		return SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_MAX;
	}

	virtual float GetAxisDeadZone(int axis)
	{
		return 0.0;
	}

	virtual EJoyAxis GetAxisMap(int axis)
	{
		switch (axis)
		{
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX:
			return EJoyAxis::JOYAXIS_Side;
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY:
			return EJoyAxis::JOYAXIS_Forward;
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX:
			return EJoyAxis::JOYAXIS_Yaw;
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY:
			return EJoyAxis::JOYAXIS_Pitch;
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT:
			return EJoyAxis::JOYAXIS_Up;
		default:
			break;
		}
		return EJoyAxis::JOYAXIS_None;
	}

	virtual const char* GetAxisName(int axis)
	{
		switch (axis)
		{
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX:
			return "Left Thumb Axis X";
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY:
			return "Left Thumb Axis Y";
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX:
			return "Right Thumb Axis X";
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY:
			return "Right Thumb Axis Y";
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT:
			return "Left Trigger";
		case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
			return "Right Trigger";
		default:
			break;
		}
		return "Unknown Axis";
	}

	virtual float GetAxisScale(int axis) 
	{ 
		return AxisSettings[axis].Multiplier;
	}

	virtual void SetAxisDeadZone(int axis, float zone) {
		AxisSettings[axis].DeadZone = clamp(zone, MIN_DEADZONE, 1.f);
	}
	virtual void SetAxisMap(int axis, EJoyAxis gameaxis) {}
	virtual void SetAxisScale(int axis, float scale) {
		AxisSettings[axis].Multiplier = scale;
	}

	bool GetEnabled()
	{
		return Enabled;
	}

	void SetEnabled(bool enabled)
	{
		Enabled = enabled;
	}

	// Used by the saver to not save properties that are at their defaults.
	virtual bool IsSensitivityDefault() { return Multiplier == 1.0f; }
	virtual bool IsAxisDeadZoneDefault(int axis) { return AxisSettings[axis].DeadZone == DEFAULT_DEADZONE; }
	virtual bool IsAxisMapDefault(int axis) { return true; }
	virtual bool IsAxisScaleDefault(int axis) { return AxisSettings[axis].Multiplier == 1.0f; }

	virtual void SetDefaultConfig()
	{
		Multiplier = 1.0f;
		for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis)
		{
			AxisSettings[axis].DeadZone = DEFAULT_DEADZONE;
			AxisSettings[axis].Multiplier = 1.0f;
		}
	}


	virtual FString GetIdentifier() { return std::to_string((unsigned long long)_Gamepad).c_str(); }

	float processAxis(SDL_GameControllerAxis axis)
	{
		uint8_t status = 0;
		double x = (double)SDL_GameControllerGetAxis(_Gamepad, axis) / (double)INT16_MAX;
		x = Joy_RemoveDeadZone(x, AxisSettings[axis].DeadZone, &status);
		return (float)x;
	}

	uint8_t processAxisAsButton(SDL_GameControllerAxis axis)
	{
		uint8_t status = 0;
		double x = (double)SDL_GameControllerGetAxis(_Gamepad, axis) / (double)INT16_MAX;
		x = Joy_RemoveDeadZone(x, AxisSettings[axis].DeadZone, &status);
		return status;
	}

	virtual void AddAxes(float axes[NUM_JOYAXIS]) 
	{
		//Movement axis
		axes[JOYAXIS_Side] = -processAxis(SDL_CONTROLLER_AXIS_LEFTX);
		axes[JOYAXIS_Forward] = -processAxis(SDL_CONTROLLER_AXIS_LEFTY);

		//Aim Axis
		axes[JOYAXIS_Yaw] = -processAxis(SDL_CONTROLLER_AXIS_RIGHTX);
		axes[JOYAXIS_Pitch] = -processAxis(SDL_CONTROLLER_AXIS_RIGHTY);

		//UP Axis
		axes[JOYAXIS_Up] = -processAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	}

	virtual void ProcessInput()
	{
		//Process all axis as buttons. I don't like it honestly, buttons are buttons

		//process left stick
		{
			float x_value = processAxis(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX);
			float y_value = processAxis(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY);

			uint8_t new_XY_status = Joy_XYAxesToButtons(x_value, y_value);
			Joy_GenerateButtonEvents(XY_status, new_XY_status, 4, KEY_PAD_LTHUMB_RIGHT);
			XY_status = new_XY_status;
		}

		//process right stick
		{
			float yaw_value = processAxis(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX);
			float pitch_value = processAxis(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY);

			uint8_t new_YawPitch_status = Joy_XYAxesToButtons(yaw_value, pitch_value);
			Joy_GenerateButtonEvents(YawPitch_status, new_YawPitch_status, 4, KEY_PAD_RTHUMB_RIGHT);
			YawPitch_status = new_YawPitch_status;
		}

		//process Left/Right Trigger
		{
			uint8_t new_Left_status = processAxisAsButton(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT) != 0 ? 1 : 0;
			Joy_GenerateButtonEvents(Left_status, new_Left_status, 1, KEY_PAD_LTRIGGER);
			Left_status = new_Left_status;

			uint8_t new_Right_status = processAxisAsButton(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT) != 0 ? 1 : 0;
			Joy_GenerateButtonEvents(Right_status, new_Right_status, 1, KEY_PAD_RTRIGGER);
			Right_status = new_Right_status;
		}

		//process DPAD
		{
			/*
				KEY_PAD_DPAD_UP			= 0x1B4,
				KEY_PAD_DPAD_DOWN		= 0x1B5,
				KEY_PAD_DPAD_LEFT		= 0x1B6,
				KEY_PAD_DPAD_RIGHT		= 0x1B7,
			*/
			uint8_t new_DPAD_status = 0;
			new_DPAD_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP);
			new_DPAD_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN) << 1;
			new_DPAD_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT) << 2;
			new_DPAD_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT) << 3;
			Joy_GenerateButtonEvents(DPAD_status, new_DPAD_status, 4, KEY_PAD_DPAD_UP);
			DPAD_status = new_DPAD_status;
		}

		//Process buttons in two sets because we handled triggers as analogs and there's an hole

		/*
			KEY_PAD_START			= 0x1B8,
			KEY_PAD_BACK			= 0x1B9,
			KEY_PAD_LTHUMB			= 0x1BA,
			KEY_PAD_RTHUMB			= 0x1BB,
			KEY_PAD_LSHOULDER		= 0x1BC,
			KEY_PAD_RSHOULDER		= 0x1BD,
		*/

		uint8_t new_Buttons1_status = 0;
		new_Buttons1_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_START);
		new_Buttons1_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_BACK) << 1;
		new_Buttons1_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSTICK) << 2;
		new_Buttons1_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSTICK) << 3;
		new_Buttons1_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER) << 4;
		new_Buttons1_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) << 5;
		Joy_GenerateButtonEvents(Buttons1_status, new_Buttons1_status, 6, KEY_PAD_START);
		Buttons1_status = new_Buttons1_status;

		/*
			KEY_PAD_A = 0x1C0,
			KEY_PAD_B = 0x1C1,
			KEY_PAD_X = 0x1C2,
			KEY_PAD_Y = 0x1C3,
		*/
		
		uint8_t new_Buttons2_status = 0;
		new_Buttons2_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A);
		new_Buttons2_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B) << 1;
		new_Buttons2_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X) << 2;
		new_Buttons2_status |= SDL_GameControllerGetButton(_Gamepad, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_Y) << 3;
		Joy_GenerateButtonEvents(Buttons2_status, new_Buttons2_status, 4, KEY_PAD_A);
		Buttons2_status = new_Buttons2_status;
	}
};

// [Nash 4 Feb 2024] seems like on Linux, the third axis is actually the Left Trigger, resulting in the player uncontrollably looking upwards.
const EJoyAxis SDLInputJoystick::DefaultAxes[5] = {JOYAXIS_Side, JOYAXIS_Forward, JOYAXIS_None, JOYAXIS_Yaw, JOYAXIS_Pitch};

class SDLInputJoystickManager
{
public:
	SDLInputJoystickManager()
	{
		for(int i = 0;i < SDL_NumJoysticks();i++)
		{
			IConfigurableJoystick *device = new SDLInputJoystick(i);

			if (SDL_IsGameController(i))
			{
				device = new SDLInputGamepad(i);
			}
			else {
				device = new SDLInputJoystick(i);
			}

			if (device->IsValid())
				Joysticks.Push(device);
			else
				delete device;
		}
	}
	~SDLInputJoystickManager()
	{
		for (unsigned int i = 0; i < Joysticks.Size(); i++)
		{
			delete Joysticks[i];
		}
	}

	void AddAxes(float axes[NUM_JOYAXIS])
	{
		for(unsigned int i = 0;i < Joysticks.Size();i++)
			Joysticks[i]->AddAxes(axes);
	}
	void GetDevices(TArray<IJoystickConfig *> &sticks)
	{
		for(unsigned int i = 0;i < Joysticks.Size();i++)
		{
			M_LoadJoystickConfig(Joysticks[i]);
			sticks.Push(Joysticks[i]);
		}
	}

	void ProcessInput() const
	{
		for(unsigned int i = 0;i < Joysticks.Size();++i)
			if(Joysticks[i]->GetEnabled()) Joysticks[i]->ProcessInput();
	}
protected:
	TArray<IConfigurableJoystick*> Joysticks;
};
static SDLInputJoystickManager *JoystickManager;

void I_StartupJoysticks()
{
#ifndef NO_SDL_JOYSTICK
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) >= 0)
	{
#ifdef _WINDOWS_UWP
		//This thread has to wait for SDL_InitSubSystem to register wginputs
		Sleep(500);
#endif
		JoystickManager = new SDLInputJoystickManager();
	}
#endif
}
void I_ShutdownInput()
{
	if(JoystickManager)
	{
		delete JoystickManager;
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	}
}

void I_GetJoysticks(TArray<IJoystickConfig *> &sticks)
{
	sticks.Clear();

	if (JoystickManager)
		JoystickManager->GetDevices(sticks);
}

void I_GetAxes(float axes[NUM_JOYAXIS])
{
	for (int i = 0; i < NUM_JOYAXIS; ++i)
	{
		axes[i] = 0;
	}
	if (use_joystick && JoystickManager)
	{
		JoystickManager->AddAxes(axes);
	}
}

void I_ProcessJoysticks()
{
	if (use_joystick && JoystickManager)
		JoystickManager->ProcessInput();
}

IJoystickConfig *I_UpdateDeviceList()
{
	return NULL;
}
