# Spec Delta

## Purpose

Maps every input device — keyboard and mouse, gamepad, touch and XR
controllers — to the same set of game actions.

## ADDED Requirements

### Requirement: Action mapping
Game code SHALL read input only as abstract actions (move, look, attack, block, jump, interact, menu) and axes, never as raw device events, and the bindings SHALL be changeable without code changes.

#### Scenario: Rebinding attack
- **WHEN** the attack action is rebound from left mouse button to key F
- **THEN** pressing F triggers an attack and the left mouse button does not

### Requirement: Keyboard, mouse and gamepad
Desktop and desktop-web builds SHALL support keyboard with mouse look and standard gamepads, with no setting needed to switch between them.

#### Scenario: Switching to gamepad mid-game
- **WHEN** the player uses the mouse and then moves a gamepad stick
- **THEN** the character moves and on-screen prompts show gamepad buttons

### Requirement: Automatic touch controls on mobile
The web build SHALL show touch controls (a virtual move stick on the left, camera drag on the right, attack, block, jump and interact buttons) on devices whose primary pointer is coarse or when a touch event is received, and SHALL hide them when keyboard or gamepad input is used.

#### Scenario: Phone shows touch controls
- **WHEN** the web build loads on a device that reports a coarse primary pointer
- **THEN** the touch controls are shown

#### Scenario: Touch moves the character
- **WHEN** the player drags the virtual stick upward
- **THEN** the character moves forward at a speed proportional to the drag distance

#### Scenario: Keyboard hides touch controls
- **WHEN** a tablet showing touch controls receives keyboard input
- **THEN** the touch controls are hidden

### Requirement: Multi-touch
Touch controls SHALL track each finger separately so moving, looking and pressing a button can happen at the same time.

#### Scenario: Move and look together
- **WHEN** one finger holds the virtual stick and another drags on the right half of the screen
- **THEN** the character moves and the camera turns in the same frame

### Requirement: Input recording and replay
The input layer SHALL be able to record the per-step action state to a file and replay it in place of live devices.

#### Scenario: Replay reproduces a run
- **WHEN** a recorded run is replayed with the same seed
- **THEN** the simulation state checksum at every step matches the recorded run
