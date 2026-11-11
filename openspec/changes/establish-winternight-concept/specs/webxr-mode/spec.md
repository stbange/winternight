# Spec Delta

## Purpose

Lets players experience Winternight in virtual reality straight from the
browser, in first person with motion-controlled melee.

## ADDED Requirements

### Requirement: Entering VR from the web build
When the browser supports immersive WebXR sessions, the web build SHALL show an "Enter VR" button that starts an immersive session, and SHALL hide the button otherwise.

#### Scenario: VR-capable browser
- **WHEN** the web build loads in a browser that supports immersive-vr sessions
- **THEN** an "Enter VR" button is shown

#### Scenario: No WebXR
- **WHEN** the web build loads in a browser without WebXR
- **THEN** no VR button is shown and the game runs normally

### Requirement: Stereo rendering in XR
During an immersive session the game SHALL render one view per eye from the poses the XR system provides, into the framebuffer the XR system provides, at the XR system's frame rate.

#### Scenario: Two views per frame
- **WHEN** a frame is rendered in an immersive session
- **THEN** the scene is drawn once for each view with that view's projection and pose

### Requirement: First-person XR play
In XR the player SHALL see in first person, move with the controller stick (smooth or snap turning, chosen in options), and attack by swinging tracked controllers, with damage based on swing speed.

#### Scenario: Swing attack
- **WHEN** the player swings the weapon controller faster than the attack threshold through an enemy
- **THEN** a hit with damage based on the swing speed is applied along the swept path

### Requirement: XR comfort
XR sessions SHALL disable vertex snapping and screen-space effects that cause discomfort, use an internal resolution suited to the headset, and offer a vignette while moving.

#### Scenario: Comfort vignette
- **WHEN** the comfort vignette option is on and the player moves with the stick
- **THEN** the edges of the view are darkened while moving

### Requirement: Leaving VR
Ending the immersive session SHALL return the game to flat-screen mode without losing game state.

#### Scenario: Exit and continue
- **WHEN** the player ends the XR session in the middle of the Prologue
- **THEN** the game continues in flat-screen mode at the same position
